#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

// Snapshot of a single tracked device in OpenVR's "standing" universe (Chaperone origin,
// +Y up / -Z forward, metres). Coords match OpenXR's XR_REFERENCE_SPACE_TYPE_STAGE, except
// that the caller must subtract m_stageFloorOffset on Y to land in OpenXR's effective
// stage space (which ReplaceStageSpace offsets to support seated calibration).
struct OpenVRTrackerPose {
    glm::vec3 position{};
    glm::fquat orientation{};
    glm::vec3 linearVelocity{};   // m/s
    glm::vec3 angularVelocity{};  // rad/s, axis-angle vector (direction=axis, magnitude=rate)
    bool valid = false;
};

enum class OpenVRClientState : uint8_t {
    NotInitialized,
    Running,
    Retrying,    // last init attempt failed; next attempt throttled to 5s
    Abandoned,  // exceeded 12 attempts; no more tries for this session
};

// Reads foot-tracker and headset poses directly from SteamVR's OpenVR layer, bypassing the
// SteamVR OpenXR bridge (which does not surface ALVR's fake Vive trackers via
// XR_HTCX_vive_tracker_interaction). Used as a fallback / alternative to the XR_HTCX path.
//
// Threading: every public method must be called from the render thread only (UpdateActions).
// OpenVR has no documented thread-safety; serialising on the render thread also keeps the
// foot data temporally aligned with hand/head poses already published by OpenXR.
//
// Lifetime: the destructor never calls VR_ShutdownInternal and never FreeLibrary on the dll
// handle. The same-process SteamVR OpenXR runtime has already initialised OpenVR, and
// shutdown would sever its connection. Abandoning the instance simply stops polling it; the
// dll handle and runtime stay alive for the rest of the process lifetime.
// Forward declarations for the private helpers so the openvr-typed structs can be referenced
// as parameters without including openvr.h in this header. Only the cpp does the full include
// (gated by OPENVR_INTERFACE_INTERNAL 1).
struct TrackedDevicePose_t;  // global-namespace C ABI type from openvr_capi.h (FnTable signature)

class OpenVRClient {
public:
    OpenVRClient();
    ~OpenVRClient();
    OpenVRClient(const OpenVRClient&) = delete;
    OpenVRClient& operator=(const OpenVRClient&) = delete;

    // Returns [LEFT, RIGHT] foot poses. Performs lazy init + retry, so safe to call before
    // any prior call. Devices that are not yet identified or currently invalid report valid=false.
    std::array<OpenVRTrackerPose, 2> PollFeet();

    // HMD (device 0) pose in the standing universe. Used by openxr.cpp for coordinate-frame
    // alignment diagnostics (compare against the cached XR head pose in m_stageSpace).
    std::optional<OpenVRTrackerPose> PollHeadset();

    OpenVRClientState GetState() const { return m_state; }
    bool IsRunning() const { return m_state == OpenVRClientState::Running; }
    const std::string& GetLoadedDllPath() const { return m_dllPath; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    OpenVRClientState m_state = OpenVRClientState::NotInitialized;
    int m_initAttempts = 0;
    std::chrono::steady_clock::time_point m_lastInitAttempt{};
    std::chrono::steady_clock::time_point m_lastScan{};
    std::string m_dllPath;

    // Lazy init. Returns true when the FnTable is ready to call. Tolerates SteamVR-not-ready
    // by transitioning to Retrying; capped at 12 attempts to avoid log spam.
    bool TryInitialize();

    // Implementation details broken out as private static helpers so the openvr-typed Impl
    // can be touched without exposing openvr types in the header.
    static std::optional<std::wstring> FindOpenVRDll(std::string& outResolution, std::string& outNotes);
    static bool ResolveEntrypoints(Impl& impl);
    static void ScanForFootTrackers(Impl& impl, bool& scannerSeenTrackers);
    static OpenVRTrackerPose ConvertPose(const TrackedDevicePose_t& p);
};