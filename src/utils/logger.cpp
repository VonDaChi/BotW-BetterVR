#include "pch.h"

#include "logger.h"

// FILE_APPEND_DATA without FILE_WRITE_DATA is what makes each write an atomic append, so the layer,
// the launcher and Cemu's stderr can share one file. Adding FILE_WRITE_DATA breaks that.
static constexpr DWORD kLogFileAccess = FILE_APPEND_DATA;
static constexpr DWORD kLogFileShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

static constexpr size_t kMaxQueuedRecords = 4096;
static constexpr DWORD kUncleanShutdownTimeoutMs = 2000;
static constexpr DWORD kFlushTimeoutMs = 500;

struct LogRecord {
    std::string text;
    LogType type = INFO;
    uint32_t threadId = 0;
    uint64_t time = 0; // FILETIME ticks
};

// Never destructed because a detached writer thread (see StopWriterThread) may still touch these
static std::mutex& s_queueMutex = *new std::mutex();
static std::condition_variable& s_wakeWriter = *new std::condition_variable();
static std::condition_variable& s_drained = *new std::condition_variable();
static std::thread s_writerThread;
static std::atomic_bool s_running = false;
static bool s_writerBusy = false;

// Reusing s_queueMutex here would self-deadlock with Start/StopWriterThread
static std::mutex s_transitionMutex;
static size_t s_instanceRefCount = 0;

// Sticky: once a writer is detached, never start another one
static constinit std::atomic_bool s_writerDetached = false;

static std::vector<LogRecord>& s_produce = *new std::vector<LogRecord>();
static std::vector<LogRecord>& s_consume = *new std::vector<LogRecord>();
static size_t s_produceCount = 0;
static uint32_t s_droppedCount = 0;

static std::string& s_batchBuffer = *new std::string();
static std::string& s_directBuffer = *new std::string();
static HANDLE s_fileHandle = INVALID_HANDLE_VALUE;
static HANDLE s_consoleHandle = NULL;
static double s_timeFrequency = 0.0;
static uint32_t s_processId = GetCurrentProcessId();

static constinit std::atomic_bool s_showTimestamps = true;
static constinit std::atomic_bool s_showThreadIds = false;

static uint64_t GetFileTimeNow() {
    FILETIME systemTime;
    GetSystemTimeAsFileTime(&systemTime);
    return ((uint64_t)systemTime.dwHighDateTime << 32) | systemTime.dwLowDateTime;
}

static const char* GetLogTypeName(LogType type) {
    switch (type) {
        case RENDERING: return "RENDER";
        case INTEROP: return "INTEROP";
        case CONTROLS: return "CONTROL";
        case PPC: return "PPC";
        case XR_DEBUGUTILS: return "XR";
        case ARROW_SHOT_CAPTURE: return "ARROW";
        case ROOMSCALE: return "ROOMSCALE";
        case INFO: return "INFO";
        case WARNING: return "WARN";
        case ERROR: return "ERROR";
        case VERBOSE: return "VERBOSE";
    }
    return "?";
}

static void FillRecord(LogRecord& record, LogType type, std::string_view message) {
    record.text.assign(message);
    record.type = type;
    record.threadId = GetCurrentThreadId();
    record.time = GetFileTimeNow();
}

static void AppendRecord(std::string& batch, const LogRecord& record) {
    if (s_showTimestamps.load(std::memory_order_relaxed)) {
        FILETIME utcTime;
        utcTime.dwLowDateTime = (DWORD)(record.time & 0xFFFFFFFFull);
        utcTime.dwHighDateTime = (DWORD)(record.time >> 32);

        FILETIME localFileTime;
        SYSTEMTIME localTime;
        if (FileTimeToLocalFileTime(&utcTime, &localFileTime) && FileTimeToSystemTime(&localFileTime, &localTime)) {
            std::format_to(std::back_inserter(batch), "[{:02}:{:02}:{:02}.{:03}] ", localTime.wHour, localTime.wMinute, localTime.wSecond, localTime.wMilliseconds);
        }
    }

    if (s_showThreadIds.load(std::memory_order_relaxed)) {
        std::format_to(std::back_inserter(batch), "[{}:{}] ", s_processId, record.threadId);
    }

    std::format_to(std::back_inserter(batch), "[{:<7}] ", GetLogTypeName(record.type));
    batch.append(record.text);
    batch.append("\n");
}

static void WriteBatch(const std::string& batch) {
    if (batch.empty()) {
        return;
    }

    if (s_fileHandle != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(s_fileHandle, batch.data(), (DWORD)batch.size(), &bytesWritten, nullptr);
    }

    if (s_consoleHandle != NULL && s_consoleHandle != INVALID_HANDLE_VALUE) {
        DWORD charsWritten = 0;
        WriteConsoleA(s_consoleHandle, batch.data(), (DWORD)batch.size(), &charsWritten, NULL);
    }

#ifdef _DEBUG
    OutputDebugStringA(batch.c_str());
#endif
}

static void DrainQueue() {
    while (true) {
        size_t count = 0;
        uint32_t dropped = 0;
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
            if (s_produceCount == 0) {
                s_writerBusy = false;
                break;
            }

            s_produce.swap(s_consume);
            count = s_produceCount;
            s_produceCount = 0;
            dropped = s_droppedCount;
            s_droppedCount = 0;
            s_writerBusy = true;
        }

        s_batchBuffer.clear();
        for (size_t index = 0; index < count; ++index) {
            AppendRecord(s_batchBuffer, s_consume[index]);
        }

        if (dropped > 0) {
            LogRecord droppedRecord;
            FillRecord(droppedRecord, WARNING, std::format("... {} log messages dropped, queue was full", dropped));
            AppendRecord(s_batchBuffer, droppedRecord);
        }

        WriteBatch(s_batchBuffer);
    }

    s_drained.notify_all();
}

static void WriterThreadMain() {
    while (s_running.load(std::memory_order_relaxed)) {
        {
            std::unique_lock<std::mutex> lock(s_queueMutex);
            s_wakeWriter.wait(lock, [] { return s_produceCount > 0 || !s_running.load(std::memory_order_relaxed); });
        }
        DrainQueue();
    }

    DrainQueue();
}

static void FlushBlocking(DWORD timeoutMs) {
    std::unique_lock<std::mutex> lock(s_queueMutex);
    if (!s_running.load(std::memory_order_relaxed)) {
        return;
    }
    s_drained.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return s_produceCount == 0 && !s_writerBusy; });
}

static void OpenLogFile() {
    // Deliberately relative: Cemu's working directory is the launcher directory, which is what puts
    // the layer and the launcher in one shared file. Module-relative would split them apart.
    std::wstring logPath = L"BetterVR_log.txt";
    if (const wchar_t* overridePath = _wgetenv(L"BETTERVR_LOG_PATH"); overridePath != nullptr && overridePath[0] != L'\0') {
        logPath = overridePath;
    }

    s_fileHandle = CreateFileW(logPath.c_str(), kLogFileAccess, kLogFileShare, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

static void StartWriterThread() {
    std::lock_guard<std::mutex> lock(s_queueMutex);
    if (s_running.load(std::memory_order_relaxed)) {
        return;
    }

    s_produce.reserve(512);
    s_consume.reserve(512);
    s_running.store(true, std::memory_order_relaxed);
    s_writerThread = std::thread(WriterThreadMain);
}

static bool StopWriterThread(DWORD timeoutMs) {
    std::thread writerThread;
    {
        std::scoped_lock lock(s_queueMutex);
        if (!s_running.load(std::memory_order_relaxed)) {
            return true;
        }
        s_running.store(false, std::memory_order_relaxed);
        writerThread = std::move(s_writerThread);
    }

    s_wakeWriter.notify_all();

    if (!writerThread.joinable()) {
        return true;
    }

    if (timeoutMs == INFINITE) {
        writerThread.join();
        return true;
    }

    // A timeout here means we're holding the loader lock, not that the writer is stuck. Detach instead of blocking forever.
    if (WaitForSingleObject(writerThread.native_handle(), timeoutMs) == WAIT_OBJECT_0) {
        writerThread.join();
        return true;
    }

    writerThread.detach();
    return false;
}


static bool ShutdownLogging(DWORD timeoutMs) {
    if (!s_running.load(std::memory_order_relaxed)) {
        return true;
    }

    Log::print<INFO>("Pausing BetterVR log writer (no active Vulkan instances)...");

    if (!StopWriterThread(timeoutMs)) {
        s_writerDetached.store(true, std::memory_order_relaxed);
        return false;
    }

    DrainQueue();

    {
        std::scoped_lock lock(s_queueMutex);
        if (s_fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(s_fileHandle);
            s_fileHandle = INVALID_HANDLE_VALUE;
        }
    }

    return true;
}

static void LogSystemHardwareInfo() {
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuid(cpuInfo, 0x80000000);
    const unsigned int maxExId = static_cast<unsigned int>(cpuInfo[0]);

    std::string cpuBrand;
    if (maxExId >= 0x80000004) {
        char brand[49] = {};
        __cpuid(reinterpret_cast<int*>(brand + 0), 0x80000002);
        __cpuid(reinterpret_cast<int*>(brand + 16), 0x80000003);
        __cpuid(reinterpret_cast<int*>(brand + 32), 0x80000004);
        cpuBrand = brand;
        while (!cpuBrand.empty() && cpuBrand.front() == ' ') cpuBrand.erase(cpuBrand.begin());
        while (!cpuBrand.empty() && cpuBrand.back() == ' ') cpuBrand.pop_back();
        if (!cpuBrand.empty()) {
            Log::print<INFO>("CPU: {}", cpuBrand.c_str());
        }
    }

    MEMORYSTATUSEX statex{};
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        const double totalGiB = double(statex.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        Log::print<INFO>("RAM: {:.2f} GiB", totalGiB);
    }
}

void Log::submit(LogType type, std::string_view message) {
    {
        std::scoped_lock lock(s_queueMutex);

        if (!s_running.load(std::memory_order_relaxed)) {
            // OPEN_ALWAYS + FILE_APPEND_DATA makes reopening safe to redo on every call
            if (s_fileHandle == INVALID_HANDLE_VALUE) {
                OpenLogFile();
            }

            LogRecord record;
            FillRecord(record, type, message);

            s_directBuffer.clear();
            AppendRecord(s_directBuffer, record);
            WriteBatch(s_directBuffer);
            return;
        }

        if (s_produceCount >= kMaxQueuedRecords) {
            s_droppedCount++;
            return;
        }

        if (s_produceCount >= s_produce.size()) {
            s_produce.emplace_back();
        }

        FillRecord(s_produce[s_produceCount++], type, message);
    }

    s_wakeWriter.notify_one();
}

Log::Log() {
    AllocConsole();
    SetConsoleTitleA("BetterVR Debugging Console");
    s_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    LARGE_INTEGER timeLI;
    QueryPerformanceFrequency(&timeLI);
    s_timeFrequency = double(timeLI.QuadPart) / 1000.0;

    OpenLogFile();
    StartWriterThread();

    Log::print<INFO>("Successfully started BetterVR!");
    LogSystemHardwareInfo();
}

Log::~Log() {
    const bool cleanShutdown = ShutdownLogging(kUncleanShutdownTimeoutMs);
    if (!cleanShutdown || s_writerDetached.load(std::memory_order_relaxed)) {
        return;
    }

    {
        std::scoped_lock lock(s_queueMutex);
        if (s_fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(s_fileHandle);
            s_fileHandle = INVALID_HANDLE_VALUE;
        }
        s_consoleHandle = NULL;
    }

    FreeConsole();
}

void Log::OnInstanceCreated() {
    std::scoped_lock lock(s_transitionMutex);
    if (s_instanceRefCount++ != 0) {
        return;
    }

    if (s_writerDetached.load(std::memory_order_relaxed)) {
        return;
    }

    {
        std::scoped_lock lock(s_queueMutex);
        if (s_fileHandle == INVALID_HANDLE_VALUE) {
            OpenLogFile();
        }
    }
    StartWriterThread();
}

void Log::OnInstanceDestroyed() {
    std::scoped_lock lock(s_transitionMutex);
    if (s_instanceRefCount == 0) {
        return;
    }
    if (--s_instanceRefCount != 0) {
        return;
    }

    ShutdownLogging(kUncleanShutdownTimeoutMs);
}

void Log::SetShowTimestamps(bool enabled) {
    s_showTimestamps.store(enabled, std::memory_order_relaxed);
}

void Log::SetShowThreadIds(bool enabled) {
    s_showThreadIds.store(enabled, std::memory_order_relaxed);
}

void Log::Flush() {
    FlushBlocking(kFlushTimeoutMs);
}

void Log::printTimeElapsed(const char* message_prefix, LARGE_INTEGER time) {
    LARGE_INTEGER timeNow;
    QueryPerformanceCounter(&timeNow);
    Log::print<INFO>("{}: {} ms", message_prefix, (double)(time.QuadPart - timeNow.QuadPart) / s_timeFrequency);
}
