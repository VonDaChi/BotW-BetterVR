#include "pch.h"
#include "openvr_client.h"

#include "utils/logger.h"

// openvr.h uses min/max macros via Windows.h and pulls in ERROR via Win32 headers.
// pch.h already #undef's ERROR / CreateEvent / CreateSemaphore; openvr.h also collides on
// min/max in some toolchain configurations, so undef them defensively right before the
// openvr include. These undefs must stay scoped to this translation unit.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// openvr.h / openvr_capi.h layout (v1.16.8) — TWO PARALLEL TYPE SYSTEMS, ABI-identical but
// distinct C++ types; MSVC treats them as unrelated (C2664 on mismatch):
//   - C++ API (openvr.h, `namespace vr`): vr::EVRInitError, vr::VRInitError_None,
//     vr::VRApplication_Background, vr::IVRSystem_Version, vr::k_unTrackedDeviceIndexInvalid.
//     Used ONLY for the VR_InitInternal2 / VR_GetGenericInterface entrypoints (C++ signatures).
//   - C ABI (openvr_capi.h, GLOBAL namespace): VR_IVRSystem_FnTable and ALL types its method
//     pointers use — TrackedDevicePose_t, k_unMaxTrackedDeviceCount, and PREFIXED enum
//     constants: ETrackingUniverseOrigin_TrackingUniverseStanding,
//     ETrackedDeviceClass_TrackedDeviceClass_GenericTracker / _HMD,
//     ETrackedDeviceProperty_Prop_RegisteredDeviceType_String,
//     ETrackedPropertyError_TrackedProp_Success. Every FnTable call must use these global
//     names — the vr:: counterparts do NOT implicitly convert to the global enum/struct types.
//   - The high-level inline wrappers (VR_Init, VR_GetVRInitErrorAsEnglishDescription overloads
//     used by C++ consumers) are gated behind `#if !defined(OPENVR_INTERFACE_INTERNAL)`; we
//     resolve everything via GetProcAddress anyway, so we never define that macro.
//
// We only need the declarations; we never link openvr_api.lib.
#include "openvr/openvr.h"
#include "openvr/openvr_capi.h"

#include <fstream>
#include <string_view>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <windows.h>

namespace {
constexpr uint32_t k_invalidDevice = static_cast<uint32_t>(vr::k_unTrackedDeviceIndexInvalid);
constexpr std::chrono::milliseconds k_initRetryInterval{ 5000 };
constexpr std::chrono::milliseconds k_scanInterval{ 2000 };
constexpr int k_maxInitAttempts = 12;

// OpenVR exports we need (resolved via GetProcAddress on openvr_api.dll). Typed as function
// pointers to avoid linking openvr_api.lib (we only need the dll at runtime).
using PFN_VR_InitInternal2 = uint32_t (*)(vr::EVRInitError*, vr::EVRApplicationType, const char*);
using PFN_VR_GetGenericInterface = void* (*)(const char*, vr::EVRInitError*);
using PFN_VR_IsHmdPresent = bool (*)();
using PFN_VR_IsRuntimeInstalled = bool (*)();
using PFN_VR_GetVRInitErrorAsEnglishDescription = const char* (*)(vr::EVRInitError);

std::string narrowPath(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), needed, nullptr, nullptr);
    return out;
}
} // namespace

// All OpenVR-side state lives here so the header can stay free of openvr types.
struct OpenVRClient::Impl {
    HMODULE dll = nullptr;
    VR_IVRSystem_FnTable* fn = nullptr;

    PFN_VR_InitInternal2 initInternal2 = nullptr;
    PFN_VR_GetGenericInterface getGenericInterface = nullptr;
    PFN_VR_IsHmdPresent isHmdPresent = nullptr;
    PFN_VR_IsRuntimeInstalled isRuntimeInstalled = nullptr;
    PFN_VR_GetVRInitErrorAsEnglishDescription getVRInitErrorAsEnglishDescription = nullptr;

    uint32_t footDeviceIndex[2] = { k_invalidDevice, k_invalidDevice };
    bool scannerSeenTrackers = false; // for diagnostic log about "no ALVR foot trackers found"
};

OpenVRClient::OpenVRClient() : m_impl(std::make_unique<Impl>()) {}

OpenVRClient::~OpenVRClient() = default; // intentionally empty: see class comment.

namespace { const char* SideName(int side) { return side == 0 ? "LEFT" : "RIGHT"; } }

// Locates an openvr_api.dll we can LoadLibrary. Prefers an already-loaded in-process copy
// (typical when the SteamVR OpenXR runtime is present) to avoid double-init conflicts.
std::optional<std::wstring> OpenVRClient::FindOpenVRDll(std::string& outResolution, std::string& outNotes) {
    outResolution.clear();
    outNotes.clear();

    // Level 1: already loaded in this process (e.g. by the SteamVR OpenXR runtime)
    if (HMODULE existing = GetModuleHandleW(L"openvr_api.dll"); existing != nullptr) {
        wchar_t rawPath[MAX_PATH * 2] = {};
        DWORD len = GetModuleFileNameW(existing, rawPath, static_cast<DWORD>(std::size(rawPath)));
        if (len > 0 && len < std::size(rawPath)) {
            outNotes = "reused in-process copy";
            return std::wstring(rawPath, len);
        }
        outNotes = "in-process copy (path unresolved)";
        return std::wstring(L"openvr_api.dll");
    }

    // Level 2: read %LOCALAPPDATA%\openvr\openvrpaths.vrpath -> runtime[0] -> bin\win64\openvr_api.dll.
    // Lightweight scan: look for "runtime" : [ " ... ", ... ] and take the first quoted string.
    {
        std::wstring vrpathPath = L"%LOCALAPPDATA%\\openvr\\openvrpaths.vrpath";
        wchar_t expanded[MAX_PATH * 2] = {};
        DWORD n = ExpandEnvironmentStringsW(vrpathPath.c_str(), expanded, static_cast<DWORD>(std::size(expanded)));
        if (n > 0 && n < std::size(expanded)) {
            std::ifstream in(expanded);
            std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (in.good() && !contents.empty()) {
                size_t key = contents.find("\"runtime\"");
                if (key != std::string::npos) {
                    size_t lb = contents.find('[', key);
                    size_t rb = (lb != std::string::npos) ? contents.find(']', lb) : std::string::npos;
                    if (lb != std::string::npos && rb != std::string::npos) {
                        size_t q1 = contents.find('"', lb);
                        size_t q2 = (q1 != std::string::npos) ? contents.find('"', q1 + 1) : std::string::npos;
                        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1) {
                            std::string firstRuntime = contents.substr(q1 + 1, q2 - q1 - 1);
                            // unescape escaped backslashes (double -> single)
                            std::string unescaped;
                            unescaped.reserve(firstRuntime.size());
                            for (size_t i = 0; i < firstRuntime.size(); ++i) {
                                if (firstRuntime[i] == '\\' && i + 1 < firstRuntime.size() && firstRuntime[i + 1] == '\\') {
                                    unescaped += '\\';
                                    ++i;
                                } else {
                                    unescaped += firstRuntime[i];
                                }
                            }
                            // runtime dir -> dir/bin/win64/openvr_api.dll
                            std::wstring wideRuntime(unescaped.begin(), unescaped.end());
                            std::wstring dllPath = wideRuntime + L"\\bin\\win64\\openvr_api.dll";
                            if (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                outNotes = "from openvrpaths.vrpath";
                                return dllPath;
                            }
                        }
                    }
                }
            }
        }
    }

    // Level 3: bare name (PATH / application dir)
    outNotes = "bare name (PATH lookup)";
    return std::wstring(L"openvr_api.dll");
}

bool OpenVRClient::ResolveEntrypoints(Impl& impl) {
    impl.initInternal2 = reinterpret_cast<PFN_VR_InitInternal2>(GetProcAddress(impl.dll, "VR_InitInternal2"));
    impl.getGenericInterface = reinterpret_cast<PFN_VR_GetGenericInterface>(GetProcAddress(impl.dll, "VR_GetGenericInterface"));
    impl.isHmdPresent = reinterpret_cast<PFN_VR_IsHmdPresent>(GetProcAddress(impl.dll, "VR_IsHmdPresent"));
    impl.isRuntimeInstalled = reinterpret_cast<PFN_VR_IsRuntimeInstalled>(GetProcAddress(impl.dll, "VR_IsRuntimeInstalled"));
    impl.getVRInitErrorAsEnglishDescription = reinterpret_cast<PFN_VR_GetVRInitErrorAsEnglishDescription>(GetProcAddress(impl.dll, "VR_GetVRInitErrorAsEnglishDescription"));
    return impl.initInternal2 && impl.getGenericInterface && impl.isHmdPresent && impl.isRuntimeInstalled && impl.getVRInitErrorAsEnglishDescription;
}

// Converts OpenVR's row-major 3x4 device-to-absolute matrix into glm position + quaternion.
// OpenVR stores mDeviceToAbsoluteTracking.m[row][col] with translation in column 3
// (m[0][3], m[1][3], m[2][3]). This matches the standard convention used by OpenGL clients.
OpenVRTrackerPose OpenVRClient::ConvertPose(const TrackedDevicePose_t& p) {
    OpenVRTrackerPose out;
    const float(&m)[3][4] = p.mDeviceToAbsoluteTracking.m;

    glm::fmat3 rot(
        m[0][0], m[1][0], m[2][0],  // column 0
        m[0][1], m[1][1], m[2][1],  // column 1
        m[0][2], m[1][2], m[2][2]); // column 2
    out.orientation = glm::quat_cast(rot);
    out.position = glm::vec3(m[0][3], m[1][3], m[2][3]);
    out.linearVelocity = glm::vec3(p.vVelocity.v[0], p.vVelocity.v[1], p.vVelocity.v[2]);
    out.angularVelocity = glm::vec3(p.vAngularVelocity.v[0], p.vAngularVelocity.v[1], p.vAngularVelocity.v[2]);
    out.valid = true;
    return out;
}

bool OpenVRClient::TryInitialize() {
    if (m_impl->fn != nullptr) return true;
    if (m_state == OpenVRClientState::Abandoned) return false;

    const auto now = std::chrono::steady_clock::now();
    if (m_state == OpenVRClientState::Retrying && (now - m_lastInitAttempt) < k_initRetryInterval) return false;

    m_lastInitAttempt = now;
    m_initAttempts++;

    if (m_initAttempts > k_maxInitAttempts) {
        m_state = OpenVRClientState::Abandoned;
        Log::print<WARNING>("[LegTrack] OpenVR backend abandoned after {} attempts (SteamVR not reachable)", k_maxInitAttempts);
        return false;
    }

    // Lazy dll load + entrypoint resolve (once)
    if (m_impl->dll == nullptr) {
        std::string resolution;
        std::string notes;
        auto path = FindOpenVRDll(resolution, notes);
        if (!path.has_value()) {
            // Path 3 (bare name) always returns a value, so this branch is defensive only.
            m_state = OpenVRClientState::Retrying;
            return false;
        }
        m_impl->dll = LoadLibraryW(path->c_str());
        if (m_impl->dll == nullptr) {
            Log::print<WARNING>("[LegTrack] OpenVR backend init attempt {} failed: LoadLibrary(\"{}\") returned NULL", m_initAttempts, narrowPath(*path));
            m_state = OpenVRClientState::Retrying;
            return false;
        }
        m_dllPath = narrowPath(*path);
        if (!ResolveEntrypoints(*m_impl)) {
            Log::print<WARNING>("[LegTrack] OpenVR backend init attempt {} failed: openvr_api.dll missing required exports (initInternal2/getGenericInterface/isHmdPresent/isRuntimeInstalled)", m_initAttempts);
            m_state = OpenVRClientState::Retrying;
            return false;
        }
    }

    // Guards refuse to spawn SteamVR if it isn't already running.
    if (!m_impl->isRuntimeInstalled() || !m_impl->isHmdPresent()) {
        // First-time only INFO; subsequent retries stay quiet to avoid log spam when SteamVR
        // is genuinely off (which is normal during the first few seconds of a Cemu boot).
        if (m_initAttempts == 1) {
            Log::print<INFO>("[LegTrack] OpenVR backend init attempt {}: SteamVR runtime not present yet (will retry quietly for up to {} attempts)", m_initAttempts, k_maxInitAttempts);
        }
        m_state = OpenVRClientState::Retrying;
        return false;
    }

    // Init: tolerate vr::VRInitError_Init_AlreadyRunning (the SteamVR OpenXR runtime may have
    // already initialised OpenVR in this process).
    vr::EVRInitError err = vr::VRInitError_None;
    m_impl->initInternal2(&err, vr::VRApplication_Background, nullptr);
    if (err != vr::VRInitError_None && err != vr::VRInitError_Init_AlreadyRunning) {
        Log::print<WARNING>("[LegTrack] OpenVR backend init attempt {} failed: VR_InitInternal2 returned error {} ({})",
            m_initAttempts, (int)err, m_impl->getVRInitErrorAsEnglishDescription(err));
        m_state = OpenVRClientState::Retrying;
        return false;
    }

    // Request the FnTable for the compile-time interface version. The vendored headers and
    // the version string are from the same openvr tag (v1.16.8 -> "IVRSystem_022"), so the
    // struct layout matches the runtime's. OpenVR promises backward compatibility, so older
    // version numbers work against newer runtimes.
    const std::string ifaceName = std::string("FnTable:") + vr::IVRSystem_Version;
    vr::EVRInitError ifaceErr = vr::VRInitError_None;
    void* table = m_impl->getGenericInterface(ifaceName.c_str(), &ifaceErr);
    if (table == nullptr || ifaceErr != vr::VRInitError_None) {
        Log::print<WARNING>("[LegTrack] OpenVR backend init attempt {} failed: VR_GetGenericInterface(\"{}\") returned null or error {}", m_initAttempts, ifaceName, (int)ifaceErr);
        m_state = OpenVRClientState::Retrying;
        return false;
    }
    m_impl->fn = reinterpret_cast<VR_IVRSystem_FnTable*>(table);
    m_state = OpenVRClientState::Running;
    Log::print<INFO>("[LegTrack] OpenVR backend init: openvr_api.dll \"{}\" (in-process copy: {})", m_dllPath, m_dllPath.find('\\') == std::string::npos ? "no (bare name resolved)" : "yes");
    return true;
}

void OpenVRClient::ScanForFootTrackers(Impl& impl, bool& scannerSeenTrackers) {
    auto& footIdx = impl.footDeviceIndex;
    uint32_t found[2] = { k_invalidDevice, k_invalidDevice };
    int trackerClassCount = 0;

    for (uint32_t idx = 0; idx < vr::k_unMaxTrackedDeviceCount; ++idx) {
        if (impl.fn->GetTrackedDeviceClass(idx) != ETrackedDeviceClass_TrackedDeviceClass_GenericTracker) continue;
        ++trackerClassCount;

        char serial[128] = {};
        ETrackedPropertyError perr = ETrackedPropertyError_TrackedProp_Success;
        impl.fn->GetStringTrackedDeviceProperty(idx, ETrackedDeviceProperty_Prop_RegisteredDeviceType_String, serial, sizeof(serial), &perr);
        if (perr != ETrackedPropertyError_TrackedProp_Success) continue;

        std::string_view s(serial);
        // Match "/devices/ALVR/tracker/left_foot" / "right_foot" (sub-string to tolerate
        // RegisteredDeviceType_String variants across SteamVR versions). Falls back to a
        // looser match on the bare foot name for non-ALVR drivers (future-proof).
        if (s.find("left_foot") != std::string_view::npos && s.find("right_foot") == std::string_view::npos) {
            found[0] = idx;
        } else if (s.find("right_foot") != std::string_view::npos) {
            found[1] = idx;
        }
    }

    for (int side = 0; side < 2; ++side) {
        if (found[side] != footIdx[side]) {
            footIdx[side] = found[side];
            if (footIdx[side] != k_invalidDevice) {
                char serial[128] = {};
                ETrackedPropertyError perr = ETrackedPropertyError_TrackedProp_Success;
                impl.fn->GetStringTrackedDeviceProperty(footIdx[side], ETrackedDeviceProperty_Prop_RegisteredDeviceType_String, serial, sizeof(serial), &perr);
                Log::print<INFO>("[LegTrack] OpenVR backend: idx={} class=Tracker serial=\"{}\" -> {}", footIdx[side], (perr == ETrackedPropertyError_TrackedProp_Success ? serial : "?"), SideName(side));
            }
        }
    }

    if (footIdx[0] == k_invalidDevice && footIdx[1] == k_invalidDevice && trackerClassCount > 0 && !scannerSeenTrackers) {
        Log::print<INFO>("[LegTrack] OpenVR backend: no foot trackers found (scanned {} GenericTracker device(s); assign LeftFoot/RightFoot role in SteamVR -> Manage Vive Trackers)", trackerClassCount);
        scannerSeenTrackers = true;
    } else if ((footIdx[0] != k_invalidDevice || footIdx[1] != k_invalidDevice) && trackerClassCount > 0) {
        // Found at least one foot; reset the "none found" sticky log.
        scannerSeenTrackers = false;
    }
}

std::array<OpenVRTrackerPose, 2> OpenVRClient::PollFeet() {
    std::array<OpenVRTrackerPose, 2> out{};

    if (!IsRunning()) {
        if (!TryInitialize()) return out;
    }

    const auto now = std::chrono::steady_clock::now();
    if (m_lastScan == std::chrono::steady_clock::time_point{} || (now - m_lastScan) >= k_scanInterval) {
        m_lastScan = now;
        ScanForFootTrackers(*m_impl, m_impl->scannerSeenTrackers);
    }

    TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount] = {};
    m_impl->fn->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, poses, k_unMaxTrackedDeviceCount);

    for (int side = 0; side < 2; ++side) {
        const uint32_t idx = m_impl->footDeviceIndex[side];
        if (idx == k_invalidDevice) continue;
        const TrackedDevicePose_t& p = poses[idx];
        if (!p.bDeviceIsConnected || !p.bPoseIsValid) continue;
        out[side] = ConvertPose(p);
    }
    return out;
}

std::optional<OpenVRTrackerPose> OpenVRClient::PollHeadset() {
    if (!IsRunning()) {
        if (!TryInitialize()) return std::nullopt;
    }

    TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount] = {};
    m_impl->fn->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, poses, k_unMaxTrackedDeviceCount);

    // The HMD is conventionally device index 0 (ETrackedDeviceClass_TrackedDeviceClass_HMD), and the FnTable does
    // not expose a "primary user" index. If device 0 is disconnected we bail rather than
    // scanning every slot for an HMD, which keeps this cheap for alignment diagnostics.
    const TrackedDevicePose_t& hmd = poses[0];
    if (!hmd.bDeviceIsConnected || !hmd.bPoseIsValid) return std::nullopt;
    if (m_impl->fn->GetTrackedDeviceClass(0) != ETrackedDeviceClass_TrackedDeviceClass_HMD) return std::nullopt;

    return ConvertPose(hmd);
}