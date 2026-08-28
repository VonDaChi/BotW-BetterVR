#pragma once

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#ifndef BETTERVR_LAUNCHER_VERSION
#define BETTERVR_LAUNCHER_VERSION "0.9.16"
#endif

namespace fs = std::filesystem;

enum class CemuMode {
    Portable,
    SettingsXml,
    AppData
};

struct LauncherPaths {
    fs::path launcherExe;
    fs::path launcherDir;
    fs::path cemuExe;
    fs::path cemuLog;
    fs::path cemuSettingsXml;
    fs::path settingsIni;
    fs::path targetBase;
    fs::path targetPack;
    fs::path downloadedGraphicsDir;
    fs::path downloadedGraphicsRules;
    fs::path downloadedGraphicsSubfolder;
    fs::path launcherLog;
    fs::path updateIgnoreFile;
    fs::path runtimeLayerDll;
    fs::path runtimeLayerJson;
    CemuMode mode = CemuMode::AppData;
};

struct OpenXRProbeResult {
    std::string runtimeName;
    std::string runtimeVersion;
    std::string systemName;
    std::string errorMessage;
    uint32_t leftWidth = 0;
    uint32_t leftHeight = 0;
    uint32_t rightWidth = 0;
    uint32_t rightHeight = 0;
};

inline std::ofstream g_launcherLog;
inline std::mutex g_launcherLogMutex;

namespace UpdateChecker {
    void CheckForUpdates(const LauncherPaths& paths);
    bool CheckCemuVersion(const LauncherPaths& paths);
}

OpenXRProbeResult ProbeOpenXR();

inline std::string Narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 1) {
        return {};
    }

    std::string result(sizeNeeded - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.empty() ? nullptr : &result[0], sizeNeeded, nullptr, nullptr);
    return result;
}

inline std::string Narrow(const fs::path& value) {
    return Narrow(value.wstring());
}

inline std::string ModeName(CemuMode mode) {
    switch (mode) {
        case CemuMode::Portable:
            return "portable";
        case CemuMode::SettingsXml:
            return "settings.xml";
        case CemuMode::AppData:
            return "appdata";
    }

    return "unknown";
}

inline void LogLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_launcherLogMutex);
    if (g_launcherLog.is_open()) {
        g_launcherLog << "[Launcher] " << line << '\n';
        g_launcherLog.flush();
    }
}

inline void LogLine(const char* line) {
    LogLine(std::string(line));
}

inline void ShowErrorMessage(const std::string& message) {
    LogLine(message);
    MessageBoxA(nullptr, message.c_str(), "BetterVR Launcher", MB_OK | MB_ICONERROR);
}

inline void ShowInfoMessage(const std::string& message) {
    LogLine(message);
    MessageBoxA(nullptr, message.c_str(), "BetterVR Launcher", MB_OK | MB_ICONINFORMATION);
}

inline fs::path GetModulePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.empty() ? nullptr : &buffer[0], static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("GetModuleFileNameW failed");
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return fs::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

inline std::optional<fs::path> GetEnvironmentPath(const wchar_t* name) {
    const wchar_t* value = _wgetenv(name);
    if (value == nullptr || value[0] == L'\0') {
        return std::nullopt;
    }

    return fs::path(value);
}

inline LauncherPaths DetectPaths() {
    LauncherPaths paths{};
    paths.launcherExe = GetModulePath();
    paths.launcherDir = paths.launcherExe.parent_path();
    paths.cemuExe = paths.launcherDir / "Cemu.exe";
    paths.settingsIni = paths.launcherDir / "BetterVR_settings.ini";
    paths.launcherLog = paths.launcherDir / "BetterVR_log.txt";
    paths.updateIgnoreFile = paths.launcherDir / "BetterVR_update_ignore_once.txt";

    if (fs::is_directory(paths.launcherDir / "portable")) {
        paths.mode = CemuMode::Portable;
        paths.targetBase = paths.launcherDir / "portable" / "graphicPacks";
        paths.cemuLog = paths.launcherDir / "portable" / "log.txt";
        paths.cemuSettingsXml = paths.launcherDir / "portable" / "settings.xml";
    }
    else if (fs::exists(paths.launcherDir / "settings.xml")) {
        paths.mode = CemuMode::SettingsXml;
        paths.targetBase = paths.launcherDir / "graphicPacks";
        paths.cemuLog = paths.launcherDir / "log.txt";
        paths.cemuSettingsXml = paths.launcherDir / "settings.xml";
    }
    else if (const std::optional<fs::path> appData = GetEnvironmentPath(L"APPDATA"); appData.has_value()) {
        paths.mode = CemuMode::AppData;
        paths.targetBase = *appData / "Cemu" / "graphicPacks";
        paths.cemuLog = *appData / "Cemu" / "log.txt";
        paths.cemuSettingsXml = *appData / "Cemu" / "settings.xml";
    }
    else {
        throw std::runtime_error("APPDATA is not available");
    }

    paths.targetPack = paths.targetBase / fs::path("BreathOfTheWild_BetterVR");
    paths.downloadedGraphicsDir = paths.targetBase / "downloadedGraphicPacks" / "BreathOfTheWild" / "Graphics";
    paths.downloadedGraphicsRules = paths.downloadedGraphicsDir / "rules.txt";
    paths.downloadedGraphicsSubfolder = paths.downloadedGraphicsDir / "NotUsedIfBetterVRPlacesRulesTxtNextToThis";
    paths.runtimeLayerDll = paths.targetPack / "BetterVR_Layer.dll";
    paths.runtimeLayerJson = paths.targetPack / "BetterVR_Layer.json";
    return paths;
}

inline void InitLog(const LauncherPaths& paths) {
    g_launcherLog.open(paths.launcherLog, std::ios::out | std::ios::trunc);
    LogLine("========================================");
    LogLine("BetterVR launcher started");
    LogLine("Version: " + std::string(BETTERVR_LAUNCHER_VERSION));
    LogLine("Launcher: " + Narrow(paths.launcherExe));
    LogLine("Cemu log.txt: " + Narrow(paths.cemuLog));
    LogLine("Cemu settings.xml: " + Narrow(paths.cemuSettingsXml));
    LogLine("Cemu mode: " + ModeName(paths.mode));
    LogLine("Graphic pack target: " + Narrow(paths.targetPack));
    LogLine("Runtime directory: " + Narrow(paths.targetPack));
}

inline void CloseLog() {
    std::lock_guard<std::mutex> lock(g_launcherLogMutex);
    if (g_launcherLog.is_open()) {
        g_launcherLog.close();
    }
}

inline void ReopenLog(const LauncherPaths& paths) {
    std::lock_guard<std::mutex> lock(g_launcherLogMutex);
    g_launcherLog.open(paths.launcherLog, std::ios::out | std::ios::app);
}

inline bool EnsureDirectory(const fs::path& directory, const char* failurePrefix) {
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        LogLine(std::string(failurePrefix) + ": " + ec.message());
        return false;
    }

    return true;
}

inline bool WriteBinaryFile(const fs::path& path, const unsigned char* data, size_t size) {
    if (!EnsureDirectory(path.parent_path(), "Failed to create parent directory")) {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        LogLine("Failed to open file for writing: " + Narrow(path));
        return false;
    }

    if (size > 0) {
        output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    if (!output.good()) {
        LogLine("Failed to write file: " + Narrow(path));
        return false;
    }

    return true;
}
