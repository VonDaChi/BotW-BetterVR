#include "pch.h"

#include "openxr.h"
#include "instance.h"

#include "utils/mod_settings.h"

#ifndef XR_BD_ULTRA_CONTROLLER_INTERACTION_EXTENSION_NAME
#define XR_BD_ULTRA_CONTROLLER_INTERACTION_EXTENSION_NAME "XR_BD_ultra_controller_interaction"
#endif

static XrBool32 XR_DebugUtilsMessengerCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageType, const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
    Log::print<XR_DEBUGUTILS>("Function {}: {}", callbackData->functionName, callbackData->message);
    return XR_FALSE;
}

OpenXR::OpenXR() {
    // Always allocate — the OpenVR backend may be selected by the user via LegTrackingBackend
    // (= OPENVr) or by AUTO when XR_HTCX fails to surface trackers. Polling before allocation
    // would force us to rebuild the client on every settings change.
    m_openvrClient = std::make_unique<OpenVRClient>();
    m_activeFootBackend = LegBackend::XrHtcx;

    uint32_t xrExtensionCount = 0;
    xrEnumerateInstanceExtensionProperties(NULL, 0, &xrExtensionCount, NULL);
    std::vector<XrExtensionProperties> instanceExtensions;
    instanceExtensions.resize(xrExtensionCount, { XR_TYPE_EXTENSION_PROPERTIES, NULL });
    {
        XrResult result = xrEnumerateInstanceExtensionProperties(NULL, xrExtensionCount, &xrExtensionCount, instanceExtensions.data());
        if (result == XR_ERROR_RUNTIME_FAILURE) {
            Log::print<ERROR>("Couldn't enumerate OpenXR extensions! Is the OpenXR runtime installed and set to the correct runtime? Restarting might help, or going to SteamVR/Oculus Link's Settings and making sure OpenXR is enabled.");
        }
        checkXRResult(result, "Couldn't enumerate OpenXR extensions!");
    }

    // Create instance with required extensions
    bool d3d12Supported = false;
    bool depthSupported = false;
    bool timeConvSupported = false;
    bool debugUtilsSupported = false;
    for (XrExtensionProperties& extensionProperties : instanceExtensions) {
        Log::print<VERBOSE>("Found available OpenXR extension: {}", extensionProperties.extensionName);
        if (strcmp(extensionProperties.extensionName, XR_KHR_D3D12_ENABLE_EXTENSION_NAME) == 0) {
            d3d12Supported = true;
        }
        if (strcmp(extensionProperties.extensionName, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME) == 0) {
            depthSupported = true;
        }
        else if (strcmp(extensionProperties.extensionName, XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME) == 0) {
            timeConvSupported = true;
        }
        else if (strcmp(extensionProperties.extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
#ifdef _DEBUG
            debugUtilsSupported = Log::isLogTypeEnabled<XR_DEBUGUTILS>();
#endif
        }
        else if (strcmp(extensionProperties.extensionName, XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME) == 0) {
            m_capabilities.supportsPicoController = true;
        }
        else if (strcmp(extensionProperties.extensionName, XR_BD_ULTRA_CONTROLLER_INTERACTION_EXTENSION_NAME) == 0) {
            m_capabilities.supportsPicoUltraController = true;
        }
        else if (strcmp(extensionProperties.extensionName, XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME) == 0) {
            m_capabilities.supportsCosmosController = true;
        }
        else if (strcmp(extensionProperties.extensionName, XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME) == 0) {
            m_capabilities.supportsHPMixedRealityController = true;
        }
        // Leg tracking: foot trackers (SteamVR trackers with a Left/Right Foot role).
        // NOTE: the macro is all-caps: XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME.
        else if (strcmp(extensionProperties.extensionName, XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME) == 0) {
            m_capabilities.supportsViveTracker = true;
        }
    }

    if (!d3d12Supported) {
        Log::print<ERROR>("OpenXR runtime doesn't support D3D12 (XR_KHR_D3D12_ENABLE)!");
        throw std::runtime_error("Current OpenXR runtime doesn't support Direct3D 12 (XR_KHR_D3D12_ENABLE). See the Github page's troubleshooting section for a solution!");
    }
    if (!depthSupported) {
        Log::print<ERROR>("OpenXR runtime doesn't support depth composition layers (XR_KHR_COMPOSITION_LAYER_DEPTH)!");
        throw std::runtime_error("Current OpenXR runtime doesn't support depth composition layers (XR_KHR_COMPOSITION_LAYER_DEPTH). See the Github page's troubleshooting section for a solution!");
    }
    if (!timeConvSupported) {
        Log::print<WARNING>("OpenXR runtime doesn't support converting time from/to XrTime (XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME). Not required, as of this version.");
    }
    if (!debugUtilsSupported && Log::isLogTypeEnabled<XR_DEBUGUTILS>()) {
        Log::print<INFO>("OpenXR runtime doesn't support debug utils (XR_EXT_DEBUG_UTILS)! Errors/debug information will no longer be able to be shown!");
    }

    std::vector<const char*> enabledExtensions = { XR_KHR_D3D12_ENABLE_EXTENSION_NAME, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME };
    if (timeConvSupported) enabledExtensions.emplace_back(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
    if (debugUtilsSupported) enabledExtensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (m_capabilities.supportsPicoController) enabledExtensions.emplace_back(XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME);
    if (m_capabilities.supportsPicoUltraController) enabledExtensions.emplace_back(XR_BD_ULTRA_CONTROLLER_INTERACTION_EXTENSION_NAME);
    if (m_capabilities.supportsCosmosController) enabledExtensions.emplace_back(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME);
    if (m_capabilities.supportsHPMixedRealityController) enabledExtensions.emplace_back(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);
    if (m_capabilities.supportsViveTracker) enabledExtensions.emplace_back(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);

    XrInstanceCreateInfo xrInstanceCreateInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    xrInstanceCreateInfo.createFlags = 0;
    xrInstanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
    xrInstanceCreateInfo.enabledExtensionNames = enabledExtensions.data();
    xrInstanceCreateInfo.enabledApiLayerCount = 0;
    xrInstanceCreateInfo.enabledApiLayerNames = NULL;
    xrInstanceCreateInfo.applicationInfo = { "BetterVR", 1, "Cemu", 1, XR_API_VERSION_1_0 };
    {
        XrResult result = XR_ERROR_RUNTIME_FAILURE;
        for (int i = 0; i < 3; i++) {
             result = xrCreateInstance(&xrInstanceCreateInfo, &m_instance);
             if (XR_SUCCEEDED(result)) {
                 break;
             }
             std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        if (result == XR_ERROR_RUNTIME_FAILURE) {
            Log::print<ERROR>("Failed to create OpenXR instance! Is the OpenXR runtime installed and set to the correct runtime? Restarting might help, or going to SteamVR/Oculus Link's Settings and making sure OpenXR is enabled.");
        }
        checkXRResult(result, "Failed to initialize the OpenXR instance!");
    }

    // Load extension pointers for this XrInstance
    xrGetInstanceProcAddr(m_instance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&func_xrGetD3D12GraphicsRequirementsKHR);
    if (timeConvSupported) {
        xrGetInstanceProcAddr(m_instance, "xrConvertTimeToWin32PerformanceCounterKHR", (PFN_xrVoidFunction*)&func_xrConvertTimeToWin32PerformanceCounterKHR);
        xrGetInstanceProcAddr(m_instance, "xrConvertWin32PerformanceCounterToTimeKHR", (PFN_xrVoidFunction*)&func_xrConvertWin32PerformanceCounterToTimeKHR);
    }
    if (debugUtilsSupported) {
        xrGetInstanceProcAddr(m_instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&func_xrCreateDebugUtilsMessengerEXT);
        xrGetInstanceProcAddr(m_instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&func_xrDestroyDebugUtilsMessengerEXT);
    }
    if (m_capabilities.supportsViveTracker) {
        xrGetInstanceProcAddr(m_instance, "xrEnumerateViveTrackerPathsHTCX", (PFN_xrVoidFunction*)&func_xrEnumerateViveTrackerPathsHTCX);
    }

    // Create debug utils messenger
    if (debugUtilsSupported) {
        XrDebugUtilsMessengerCreateInfoEXT utilsMessengerCreateInfo = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        utilsMessengerCreateInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        utilsMessengerCreateInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
        utilsMessengerCreateInfo.userCallback = &XR_DebugUtilsMessengerCallback;
        func_xrCreateDebugUtilsMessengerEXT(m_instance, &utilsMessengerCreateInfo, &m_debugMessengerHandle);
    }

    // Get system information
    XrSystemGetInfo xrSystemGetInfo = { XR_TYPE_SYSTEM_GET_INFO };
    xrSystemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    checkXRResult(xrGetSystem(m_instance, &xrSystemGetInfo, &m_systemId), "No (available) head mounted display found!");

    XrSystemProperties xrSystemProperties = { XR_TYPE_SYSTEM_PROPERTIES };
    checkXRResult(xrGetSystemProperties(m_instance, m_systemId, &xrSystemProperties), "Couldn't get system properties of the given VR headset!");
    m_capabilities.supportsOrientational = xrSystemProperties.trackingProperties.orientationTracking;
    m_capabilities.supportsPositional = xrSystemProperties.trackingProperties.positionTracking;

    XrInstanceProperties properties = { XR_TYPE_INSTANCE_PROPERTIES };
    checkXRResult(xrGetInstanceProperties(m_instance, &properties), "Failed to get runtime details using xrGetInstanceProperties!");

    XrViewConfigurationProperties stereoViewConfiguration = { XR_TYPE_VIEW_CONFIGURATION_PROPERTIES };
    checkXRResult(xrGetViewConfigurationProperties(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &stereoViewConfiguration), "There's no VR headset available that allows stereo rendering!");
    m_capabilities.supportsMutatableFOV = stereoViewConfiguration.fovMutable;

    XrGraphicsRequirementsD3D12KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
    checkXRResult(func_xrGetD3D12GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements), "Couldn't get D3D12 requirements for the given VR headset!");
    m_capabilities.adapter = graphicsRequirements.adapterLuid;
    m_capabilities.minFeatureLevel = graphicsRequirements.minFeatureLevel;

    // Print configuration used, mostly for debugging purposes
    Log::print<INFO>("Acquired system to be used:");
    Log::print<INFO>(" - System Name: {}", xrSystemProperties.systemName); // Oculus Quest2
    Log::print<INFO>(" - Runtime Name: {}", properties.runtimeName); // Oculus
    Log::print<INFO>(" - Runtime Version: {}.{}.{}", XR_VERSION_MAJOR(properties.runtimeVersion), XR_VERSION_MINOR(properties.runtimeVersion), XR_VERSION_PATCH(properties.runtimeVersion));
    Log::print<INFO>(" - Supports Mutable FOV: {}", m_capabilities.supportsMutatableFOV ? "Yes" : "No");
    Log::print<INFO>(" - Supports Orientation Tracking: {}", xrSystemProperties.trackingProperties.orientationTracking ? "Yes" : "No");
    Log::print<INFO>(" - Supports Positional Tracking: {}", xrSystemProperties.trackingProperties.positionTracking ? "Yes" : "No");
    Log::print<INFO>(" - Supports D3D12 feature level {} or higher", graphicsRequirements.minFeatureLevel);

    m_capabilities.isOculusLinkRuntime = std::string(properties.runtimeName) == "Oculus";
    Log::print<INFO>(" - Using Meta Quest Link OpenXR runtime: {}", m_capabilities.isOculusLinkRuntime ? "Yes" : "No");
    
    m_capabilities.isMetaSimulator = std::string(properties.runtimeName).find("Meta XR Simulator") != std::string::npos;
}

OpenXR::~OpenXR() {
    this->m_renderer.reset();

    for (XrSpace& handSpace : m_inGameHandSpaces) {
        if (handSpace != XR_NULL_HANDLE) {
            xrDestroySpace(handSpace);
            handSpace = XR_NULL_HANDLE;
        }
    }
    for (XrSpace& aimSpace : m_inGameAimSpaces) {
        if (aimSpace != XR_NULL_HANDLE) {
            xrDestroySpace(aimSpace);
            aimSpace = XR_NULL_HANDLE;
        }
    }
    for (XrSpace& handSpace : m_inMenuHandSpaces) {
        if (handSpace != XR_NULL_HANDLE) {
            xrDestroySpace(handSpace);
            handSpace = XR_NULL_HANDLE;
        }
    }
    for (XrSpace& aimSpace : m_inMenuAimSpaces) {
        if (aimSpace != XR_NULL_HANDLE) {
            xrDestroySpace(aimSpace);
            aimSpace = XR_NULL_HANDLE;
        }
    }

    if (m_headSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_headSpace);
    }

    if (m_stageSpace != XR_NULL_HANDLE) {
        xrDestroySpace(m_stageSpace);
    }

    for (XrSpace retiredSpace : m_retiredStageSpaces) {
        xrDestroySpace(retiredSpace);
    }
    m_retiredStageSpaces.clear();

    if (m_session != XR_NULL_HANDLE) {
        xrDestroySession(m_session);
    }

    if (m_debugMessengerHandle != XR_NULL_HANDLE) {
        func_xrDestroyDebugUtilsMessengerEXT(m_debugMessengerHandle);
    }

    if (m_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(m_instance);
    }
}

std::array<XrViewConfigurationView, 2> OpenXR::GetViewConfigurations() {
    uint32_t eyeViewsConfigurationCount = 0;
    checkXRResult(xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &eyeViewsConfigurationCount, nullptr), "Can't get number of individual views for stereo view available");
    checkAssert(eyeViewsConfigurationCount == 2, std::format("Expected 2 views for the stereo configuration but got {} which is unsupported!", eyeViewsConfigurationCount).c_str());

    std::array<XrViewConfigurationView, 2> xrViewConf = { XrViewConfigurationView{ XR_TYPE_VIEW_CONFIGURATION_VIEW }, XrViewConfigurationView{ XR_TYPE_VIEW_CONFIGURATION_VIEW } };
    checkXRResult(xrEnumerateViewConfigurationViews(m_instance, m_systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, eyeViewsConfigurationCount, &eyeViewsConfigurationCount, xrViewConf.data()), "Can't get individual views for stereo view available!");

    Log::print<INFO>("Swapchain configuration to be used:");
    Log::print<INFO>(" - [Left] Max View Resolution: w={}, h={} with {} samples", xrViewConf[0].maxImageRectWidth, xrViewConf[0].maxImageRectHeight, xrViewConf[0].maxSwapchainSampleCount);
    Log::print<INFO>(" - [Right] Max View Resolution: w={}, h={}  with {} samples", xrViewConf[1].maxImageRectWidth, xrViewConf[1].maxImageRectHeight, xrViewConf[1].maxSwapchainSampleCount);
    Log::print<INFO>(" - [Left] Recommended View Resolution: w={}, h={}  with {} samples", xrViewConf[0].recommendedImageRectWidth, xrViewConf[0].recommendedImageRectHeight, xrViewConf[0].recommendedSwapchainSampleCount);
    Log::print<INFO>(" - [Right] Recommended View Resolution: w={}, h={}  with {} samples", xrViewConf[1].recommendedImageRectWidth, xrViewConf[1].recommendedImageRectHeight, xrViewConf[0].recommendedSwapchainSampleCount);
    return xrViewConf;
}

void OpenXR::CreateSession(const XrGraphicsBindingD3D12KHR& d3d12Binding) {
    Log::print<INFO>("Creating the OpenXR session...");

    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionCreateInfo.systemId = m_systemId;
    sessionCreateInfo.next = &d3d12Binding;
    sessionCreateInfo.createFlags = 0;
    checkXRResult(xrCreateSession(m_instance, &sessionCreateInfo, &m_session), "Failed to create Vulkan-based OpenXR session!");

    Log::print<INFO>("Creating the OpenXR spaces...");
    m_hasSeatedHeightCalibration = false;
    m_seenFirstLoadingScreen = false;
    m_lastSeenPlayMode.reset();
    ReplaceStageSpace(0.0f);

    XrReferenceSpaceCreateInfo headSpaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    headSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    headSpaceCreateInfo.poseInReferenceSpace = s_xrIdentityPose;
    checkXRResult(xrCreateReferenceSpace(m_session, &headSpaceCreateInfo, &m_headSpace), "Failed to create reference space for head!");
}

// floorOffset is where the space's origin sits above the runtime's floor, so every located pose shifts together
void OpenXR::ReplaceStageSpace(float floorOffset) {
    XrReferenceSpaceCreateInfo stageSpaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    stageSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    stageSpaceCreateInfo.poseInReferenceSpace = s_xrIdentityPose;
    stageSpaceCreateInfo.poseInReferenceSpace.position.y = floorOffset;

    XrSpace newStageSpace = XR_NULL_HANDLE;
    checkXRResult(xrCreateReferenceSpace(m_session, &stageSpaceCreateInfo, &newStageSpace), "Failed to create reference space for stage!");

    // other threads may still be locating against the old handle, so it is only released with the session
    if (m_stageSpace != XR_NULL_HANDLE) {
        m_retiredStageSpaces.push_back(m_stageSpace);
    }
    m_stageSpace = newStageSpace;
    m_stageFloorOffset = floorOffset;
}

// Link's eye level above his feet while standing, tuned in-game (his bind pose puts them at 1.59m)
static constexpr float kLinkEyeHeight = 1.65f;

void OpenXR::RequestSeatedHeightCalibration(XrTime notBeforeTime) {
    m_seatedHeightCalibrationNotBeforeTime.store(notBeforeTime, std::memory_order_relaxed);
    m_seatedHeightCalibrationRequested.store(true, std::memory_order_release);
}

std::optional<float> OpenXR::GetCalibratedSeatedEyeHeight() const {
    if (!m_hasSeatedHeightCalibration) {
        return std::nullopt;
    }
    return kLinkEyeHeight + m_stageFloorOffset;
}

void OpenXR::UpdateSeatedHeightCalibration(XrTime predictedDisplayTime, const std::optional<XrSpaceLocation>& headLocation) {
    const PlayMode playMode = GetSettings().GetPlayMode();
    if (m_lastSeenPlayMode.has_value() && playMode != m_lastSeenPlayMode.value() && playMode == PlayMode::SEATED) {
        RequestSeatedHeightCalibration();
    }
    m_lastSeenPlayMode = playMode;

    if (!m_seenFirstLoadingScreen && CemuHooks::IsScreenVisible(ScreenId::LoadingWeapon_00)) {
        m_seenFirstLoadingScreen = true;
        if (playMode == PlayMode::SEATED) {
            RequestSeatedHeightCalibration();
        }
    }

    if (playMode != PlayMode::SEATED) {
        m_seatedHeightCalibrationRequested.store(false, std::memory_order_relaxed);
        if (m_stageFloorOffset != 0.0f) {
            ReplaceStageSpace(0.0f);
            Log::print<INFO>("Seated height calibration cleared, using the real height above the floor again");
        }
        m_hasSeatedHeightCalibration = false;
        return;
    }

    if (!m_seatedHeightCalibrationRequested.load(std::memory_order_acquire)) {
        return;
    }
    if (predictedDisplayTime < m_seatedHeightCalibrationNotBeforeTime.load(std::memory_order_relaxed)) {
        return;
    }
    if (!headLocation.has_value() || !std::isfinite(headLocation->pose.position.y)) {
        return;
    }

    const float realEyeHeight = headLocation->pose.position.y + m_stageFloorOffset;
    m_seatedHeightCalibrationRequested.store(false, std::memory_order_relaxed);
    ReplaceStageSpace(realEyeHeight - kLinkEyeHeight);
    m_hasSeatedHeightCalibration = true;
    Log::print<INFO>("Seated height calibrated: eyes are {:.2f}m above the floor, stage origin moved by {:.2f}m", realEyeHeight, m_stageFloorOffset);
}

void OpenXR::CreateActions() {
    Log::print<INFO>("Creating the OpenXR actions...");

    m_handPaths = { GetXRPath("/user/hand/left"), GetXRPath("/user/hand/right") };

    // Creates an action bound to an explicit set of subaction paths. Hand actions use
    // /user/hand/{left,right}, while the foot tracker pose action needs
    // /user/vive_tracker_htcx/role/{left_foot,right_foot} instead.
    auto createActionWithPaths = [this](const XrActionSet& actionSet, const char* id, const char* name, XrActionType actionType, XrAction& action, const std::array<XrPath, 2>& subactionPaths) {
        XrActionCreateInfo actionInfo = { XR_TYPE_ACTION_CREATE_INFO };
        actionInfo.actionType = actionType;
        strncpy_s(actionInfo.actionName, id, XR_MAX_ACTION_NAME_SIZE-1);
        strncpy_s(actionInfo.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE-1);
        actionInfo.countSubactionPaths = (uint32_t)subactionPaths.size();
        actionInfo.subactionPaths = subactionPaths.data();
        checkXRResult(xrCreateAction(actionSet, &actionInfo, &action), std::format("Failed to create action for {}", id).c_str());
    };
    auto createAction = [this, &createActionWithPaths](const XrActionSet& actionSet, const char* id, const char* name, XrActionType actionType, XrAction& action) {
        createActionWithPaths(actionSet, id, name, actionType, action, m_handPaths);
    };

    {
        XrActionSetCreateInfo actionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(actionSetInfo.actionSetName, "gameplay_fps");
        strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
        actionSetInfo.priority = 0;
        checkXRResult(xrCreateActionSet(m_instance, &actionSetInfo, &m_gameplayActionSet), "Failed to create controller actions for gameplay_fps!");

        createAction(m_gameplayActionSet, "pose", "Grip Pose", XR_ACTION_TYPE_POSE_INPUT, m_inGameGripPoseAction);
        createAction(m_gameplayActionSet, "aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, m_inGameAimPoseAction);
        createAction(m_gameplayActionSet, "move", "Move", XR_ACTION_TYPE_VECTOR2F_INPUT, m_moveAction);
        createAction(m_gameplayActionSet, "camera", "Camera Rotation", XR_ACTION_TYPE_VECTOR2F_INPUT, m_cameraAction);
        createAction(m_gameplayActionSet, "ingame_modmenu", "Mod Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, m_inGame_modMenuAction);

        createAction(m_gameplayActionSet, "grab_interact", "Interact / Pick up objects from floor or weapon from body slots", XR_ACTION_TYPE_FLOAT_INPUT, m_grab_interactAction);
        createAction(m_gameplayActionSet, "jump", "Jump", XR_ACTION_TYPE_BOOLEAN_INPUT, m_jumpAction);
        createAction(m_gameplayActionSet, "run_interact_cancel", "Interact (Quick press) - Run/Cancel Interaction (Long press)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_run_interactAction);
        createAction(m_gameplayActionSet, "userune_dpadmenu", "Use Rune (Quick press) - Dpad menu (Long press)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_useRune_dpadMenu_Action);

        createAction(m_gameplayActionSet, "userighthanditem", "Use/Attack/Throw item held in right hand (Melee attacks/Draw bow/Throw object)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_useRightItemAction);
        createAction(m_gameplayActionSet, "uselefthanditem", "Use item held in left hand (Rune/Shield Parry)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_useLeftItemAction);

        createAction(m_gameplayActionSet, "crouch_scope", "Crouch (Quick press) - Open Scope (Long press)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_crouch_scopeAction);
        createAction(m_gameplayActionSet, "ingame_inventory_map", "Open Inventory (Quick press) - Open Map (Long press)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_inGame_inventory_mapAction);

        createAction(m_gameplayActionSet, "rumble", "Rumble", XR_ACTION_TYPE_VIBRATION_OUTPUT, m_rumbleAction);

        // Leg tracking: the foot pose action must be created with the tracker subaction paths,
        // not the hand paths, otherwise xrGetActionStatePose will never report isActive.
        if (m_capabilities.supportsViveTracker) {
            m_footPaths = {
                GetXRPath("/user/vive_tracker_htcx/role/left_foot"),
                GetXRPath("/user/vive_tracker_htcx/role/right_foot")
            };
            createActionWithPaths(m_gameplayActionSet, "foot_pose", "Foot Pose", XR_ACTION_TYPE_POSE_INPUT, m_footPoseAction, m_footPaths);
        }
    }

    {
        XrActionSetCreateInfo actionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(actionSetInfo.actionSetName, "menu");
        strcpy_s(actionSetInfo.localizedActionSetName, "Menu Navigation");
        actionSetInfo.priority = 0;
        checkXRResult(xrCreateActionSet(m_instance, &actionSetInfo, &m_menuActionSet), "Failed to create controller bindings for the menu!");

        createAction(m_menuActionSet, "pose", "Grip Pose", XR_ACTION_TYPE_POSE_INPUT, m_inMenuGripPoseAction);
        createAction(m_menuActionSet, "aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, m_inMenuAimPoseAction);

        createAction(m_menuActionSet, "scroll", "Scroll (Right Thumbstick)", XR_ACTION_TYPE_VECTOR2F_INPUT, m_scrollAction);
        createAction(m_menuActionSet, "navigate", "Navigate (Left Thumbstick)", XR_ACTION_TYPE_VECTOR2F_INPUT, m_navigateAction);
        createAction(m_menuActionSet, "select", "Select (A Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_selectAction);
        createAction(m_menuActionSet, "cancel", "Back/Cancel (B Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_backAction);
        createAction(m_menuActionSet, "sort", "Sort (Y Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_sortAction);
        createAction(m_menuActionSet, "hold", "Hold (X Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_holdAction);
        createAction(m_menuActionSet, "left_grip", "Switch To Left Tab (L Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_leftGripAction);
        createAction(m_menuActionSet, "right_grip", "Switch To Right Tab (R Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_rightGripAction);
        createAction(m_menuActionSet, "lefttrigger", "Left Trigger", XR_ACTION_TYPE_BOOLEAN_INPUT, m_leftTriggerAction);
        createAction(m_menuActionSet, "righttrigger", "Right Trigger", XR_ACTION_TYPE_BOOLEAN_INPUT, m_rightTriggerAction);

        createAction(m_menuActionSet, "inmenu_inventory_map", "Close Inventory (Wii U - Start Button) - Close Map (Wii U - Select Button)", XR_ACTION_TYPE_BOOLEAN_INPUT, m_inMenu_inventory_mapAction);
        createAction(m_menuActionSet, "inmenu_modmenu", "Mod Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, m_inMenu_modMenuAction);
    }

    {
        ControllerActionBindings bindings;
        bindings.inGameGripPoseAction = m_inGameGripPoseAction;
        bindings.inGameAimPoseAction = m_inGameAimPoseAction;
        bindings.inMenuGripPoseAction = m_inMenuGripPoseAction;
        bindings.inMenuAimPoseAction = m_inMenuAimPoseAction;
        bindings.moveAction = m_moveAction;
        bindings.cameraAction = m_cameraAction;
        bindings.grab_interactAction = m_grab_interactAction;
        bindings.jumpAction = m_jumpAction;
        bindings.run_interactAction = m_run_interactAction;
        bindings.useRune_dpadMenu_Action = m_useRune_dpadMenu_Action;
        bindings.inGame_modMenuAction = m_inGame_modMenuAction;
        bindings.useLeftItemAction = m_useLeftItemAction;
        bindings.useRightItemAction = m_useRightItemAction;
        bindings.crouch_scopeAction = m_crouch_scopeAction;
        bindings.inGame_inventory_mapAction = m_inGame_inventory_mapAction;
        bindings.rumbleAction = m_rumbleAction;
        bindings.scrollAction = m_scrollAction;
        bindings.navigateAction = m_navigateAction;
        bindings.selectAction = m_selectAction;
        bindings.backAction = m_backAction;
        bindings.sortAction = m_sortAction;
        bindings.holdAction = m_holdAction;
        bindings.leftGripAction = m_leftGripAction;
        bindings.rightGripAction = m_rightGripAction;
        bindings.leftTriggerAction = m_leftTriggerAction;
        bindings.rightTriggerAction = m_rightTriggerAction;
        bindings.inMenu_modMenuAction = m_inMenu_modMenuAction;
        bindings.inMenu_inventory_mapAction = m_inMenu_inventory_mapAction;
        bindings.footPoseAction = m_footPoseAction;

        SuggestControllerBindings(m_instance, bindings, m_capabilities.supportsPicoController, m_capabilities.supportsPicoUltraController, m_capabilities.supportsCosmosController, m_capabilities.supportsHPMixedRealityController, m_capabilities.supportsViveTracker);
    }

    XrSessionActionSetsAttachInfo attachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
    std::array actionSets = { m_gameplayActionSet, m_menuActionSet };
    attachInfo.countActionSets = (uint32_t)actionSets.size();
    attachInfo.actionSets = actionSets.data();
    checkXRResult(xrAttachSessionActionSets(m_session, &attachInfo), "Failed to attach action sets to session!");

    for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
        XrActionSpaceCreateInfo createInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
        createInfo.action = m_inGameGripPoseAction;
        createInfo.subactionPath = m_handPaths[side];
        createInfo.poseInActionSpace = s_xrIdentityPose;
        checkXRResult(xrCreateActionSpace(m_session, &createInfo, &m_inGameHandSpaces[side]), "Failed to create action space for hand pose!");

        createInfo.action = m_inGameAimPoseAction;
        checkXRResult(xrCreateActionSpace(m_session, &createInfo, &m_inGameAimSpaces[side]), "Failed to create action space for aim pose!");
    }

    for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
        XrActionSpaceCreateInfo createInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
        createInfo.action = m_inMenuGripPoseAction;
        createInfo.subactionPath = m_handPaths[side];
        createInfo.poseInActionSpace = s_xrIdentityPose;
        checkXRResult(xrCreateActionSpace(m_session, &createInfo, &m_inMenuHandSpaces[side]), "Failed to create action space for hand pose!");

        createInfo.action = m_inMenuAimPoseAction;
        checkXRResult(xrCreateActionSpace(m_session, &createInfo, &m_inMenuAimSpaces[side]), "Failed to create action space for menu aim pose!");
    }

    // Leg tracking: one action space per foot tracker
    if (m_capabilities.supportsViveTracker && m_footPoseAction != XR_NULL_HANDLE) {
        for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
            XrActionSpaceCreateInfo createInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
            createInfo.action = m_footPoseAction;
            createInfo.subactionPath = m_footPaths[side];
            createInfo.poseInActionSpace = s_xrIdentityPose;
            checkXRResult(xrCreateActionSpace(m_session, &createInfo, &m_footSpaces[side]), "Failed to create action space for foot pose!");
        }
    }

    // initialize rumble manager
    m_rumbleManager = std::make_unique<RumbleManager>(m_session, m_rumbleAction);
    m_rumbleManager.get()->initializeXrPathsAndStartTime(m_instance);

    m_capabilities.activeControllerType = DetectActiveControllerType(m_instance, m_session);

    LogAvailableViveTrackerPaths();
}

void OpenXR::LogAvailableViveTrackerPaths() {
    if (!m_capabilities.supportsViveTracker) {
        Log::print<INFO>("[LegTrack] Foot tracking unavailable: runtime lacks {}", XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);
        return;
    }
    if (func_xrEnumerateViveTrackerPathsHTCX == nullptr) {
        Log::print<WARNING>("[LegTrack] xrEnumerateViveTrackerPathsHTCX could not be loaded - cannot list tracker roles");
        return;
    }

    uint32_t pathCount = 0;
    XrResult result = func_xrEnumerateViveTrackerPathsHTCX(m_instance, 0, &pathCount, nullptr);
    if (result != XR_SUCCESS || pathCount == 0) {
        Log::print<INFO>("[LegTrack] Runtime exposes {} tracker path(s) (result={}). Assign the Left/Right Foot role in SteamVR -> Manage Vive Trackers if you expect foot trackers.", pathCount, (int)result);
        return;
    }

    std::vector<XrViveTrackerPathsHTCX> trackerPaths(pathCount, XrViveTrackerPathsHTCX{ XR_TYPE_VIVE_TRACKER_PATHS_HTCX });
    result = func_xrEnumerateViveTrackerPathsHTCX(m_instance, pathCount, &pathCount, trackerPaths.data());
    if (result != XR_SUCCESS) {
        Log::print<WARNING>("[LegTrack] Failed to enumerate tracker paths (result={})", (int)result);
        return;
    }

    Log::print<INFO>("[LegTrack] Runtime exposes {} tracker path(s):", pathCount);
    for (const XrViveTrackerPathsHTCX& trackerPath : trackerPaths) {
        char persistentPathBuffer[XR_MAX_PATH_LENGTH] = {};
        char rolePathBuffer[XR_MAX_PATH_LENGTH] = {};
        uint32_t bufferLength = 0;
        xrPathToString(m_instance, trackerPath.persistentPath, (uint32_t)sizeof(persistentPathBuffer), &bufferLength, persistentPathBuffer);
        bufferLength = 0;
        xrPathToString(m_instance, trackerPath.rolePath, (uint32_t)sizeof(rolePathBuffer), &bufferLength, rolePathBuffer);
        Log::print<INFO>("[LegTrack]   tracker: persistentPath={} rolePath={}", persistentPathBuffer, rolePathBuffer);
    }
}

void OpenXR::EnsureViveTrackerBindingsSuggested() {
    if (m_footTrackerBindingsSuggested) {
        return;
    }
    if (m_footPoseAction == XR_NULL_HANDLE) {
        return;
    }

    // Same bindings as the startup-time SuggestControllerBindings call (controller_bindings.h:329-340),
    // re-issued because the runtime may have rejected them earlier when xrEnumerateViveTrackerPathsHTCX
    // returned 0 trackers. With trackers now exposed, the runtime is expected to accept them.
    std::array footBindings = {
        XrActionSuggestedBinding{ .action = m_footPoseAction, .binding = GetXRPath("/user/vive_tracker_htcx/role/left_foot/input/grip/pose") },
        XrActionSuggestedBinding{ .action = m_footPoseAction, .binding = GetXRPath("/user/vive_tracker_htcx/role/right_foot/input/grip/pose") },
    };

    XrInteractionProfileSuggestedBinding suggestedBindingsInfo = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
    suggestedBindingsInfo.interactionProfile = GetXRPath("/interaction_profiles/htcx/vive_tracker_htcx");
    suggestedBindingsInfo.countSuggestedBindings = (uint32_t)footBindings.size();
    suggestedBindingsInfo.suggestedBindings = footBindings.data();

    XrResult result = xrSuggestInteractionProfileBindings(m_instance, &suggestedBindingsInfo);
    if (result == XR_SUCCESS) {
        m_footTrackerBindingsSuggested = true;
        Log::print<INFO>("[LegTrack] Re-suggested HTCX Vive Tracker bindings (left/right foot grip pose).");
        return;
    }
    if (result == XR_ERROR_PATH_UNSUPPORTED) {
        // Terminal: the runtime knows about the extension + trackers exist, but still rejects the
        // bindings. Mark as suggested so we stop retrying every second.
        m_footTrackerBindingsSuggested = true;
        Log::print<WARNING>("[LegTrack] xrSuggestInteractionProfileBindings rejected HTCX Vive Tracker profile (PATH_UNSUPPORTED) even though trackers are exposed. The runtime won't bridge these trackers to OpenXR; an OpenVR backend will be required for foot tracking.");
        return;
    }
    Log::print<WARNING>("[LegTrack] xrSuggestInteractionProfileBindings for HTCX Vive Tracker profile failed: {}", (int)result);
}

// Backfills the per-frame foot-pose fields from OpenVR's TrackedDevicePose_t so the consumer
// (leg motion analyser in controls.cpp) sees the same XrSpaceLocation/XrSpaceVelocity layout it
// would have received from XR_HTCX. OpenVR is in the same chaperone origin as OpenXR's stage
// space, both use Y-up metres and -Z forward, so no further rotation is needed — only the
// "stage floor" Y offset has to be subtracted (XR_REFERENCE_SPACE_TYPE_STAGE floors at Y=0,
// OpenVR's TrackingUniverseStanding also floors there, but BetterVR's ReplaceStageSpace adds
// m_stageFloorOffset so seated users can lower the floor; keep the convention).
//
// Caller passes the same newState it intends to publish via m_input (UpdateActions holds a
// non-const InputState reference) so the write goes straight into the publish-ready buffer
// rather than reloading m_input and racing with another frame's publish.
void OpenXR::WriteFootFromOpenVR(EyeSide side, const OpenVRTrackerPose& pose, InputState& newState) {
    XrSpaceLocation& loc = newState.shared.footPoseLocation[side];
    XrSpaceVelocity& vel = newState.shared.footPoseVelocity[side];
    XrActionStatePose& st = newState.shared.footPose[side];

    loc.pose.orientation = { pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w };
    // Subtract the stage floor offset so the consumer sees the same Y as XR_HTCX's m_stageSpace.
    loc.pose.position = { pose.position.x, pose.position.y - m_stageFloorOffset, pose.position.z };
    loc.locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

    vel.linearVelocity = { pose.linearVelocity.x, pose.linearVelocity.y, pose.linearVelocity.z };
    vel.angularVelocity = { pose.angularVelocity.x, pose.angularVelocity.y, pose.angularVelocity.z };
    vel.velocityFlags = XR_SPACE_VELOCITY_LINEAR_VALID_BIT | XR_SPACE_VELOCITY_ANGULAR_VALID_BIT;

    // XrActionStatePose has no isPressed (only isActive); pose actions don't model a "pressed"
    // axis. POSE_INPUT reports isActive=true whenever the source can supply a pose, regardless
    // of any user input.
    st.isActive = XR_TRUE;
}

// Throttled (1 Hz) leg-tracking diagnostic. Prints the active backend, the data source for each
// side, and—if available—XYZ + horizontal speed. Also re-polls XR_HTCX tracker enumeration at
// 1 Hz (it lives here, not in UpdateActions, because the runtime may start exposing trackers
// long after the startup-time SuggestControllerBindings call).
void OpenXR::LogFootDiagnostics(const InputState& newState) {
    static std::chrono::steady_clock::time_point s_lastFootLogTime{};
    const auto now = std::chrono::steady_clock::now();
    if ((now - s_lastFootLogTime) < std::chrono::milliseconds(1000)) return;
    s_lastFootLogTime = now;

    const char* backendLabel = (m_activeFootBackend == LegBackend::OpenVr) ? "openvr" : "xr_htcx";
    static const char* kOpenVRStateNames[] = { "NotInitialized", "Running", "Retrying", "Abandoned" };
    const int openvrStateIdx = (int)m_openvrClient->GetState();
    const char* openvrStateName = (openvrStateIdx >= 0 && openvrStateIdx <= 3) ? kOpenVRStateNames[openvrStateIdx] : "?";
    Log::print<INFO>("[LegTrack] backend={} openvrState={}({})", backendLabel, openvrStateIdx, openvrStateName);

    // XR_HTCX-only: re-poll runtime for newly-exposed trackers once per second. If the binding
    // was never accepted and we just saw pathCount turn non-zero, retry the bindings.
    if (m_activeFootBackend == LegBackend::XrHtcx && func_xrEnumerateViveTrackerPathsHTCX != nullptr) {
        uint32_t pathCount = 0;
        XrResult enumResult = func_xrEnumerateViveTrackerPathsHTCX(m_instance, 0, &pathCount, nullptr);
        Log::print<INFO>("[LegTrack] Enumerate: pathCount={} (result={})", pathCount, (int)enumResult);
        if (enumResult == XR_SUCCESS && pathCount > 0 && !m_footTrackerBindingsSuggested) {
            EnsureViveTrackerBindingsSuggested();
        }
    }

    for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
        const char* sideName = (side == EyeSide::LEFT) ? "L" : "R";
        const XrActionStatePose& footPose = newState.shared.footPose[side];
        const XrSpaceLocation& footLocation = newState.shared.footPoseLocation[side];
        const XrSpaceVelocity& footVelocity = newState.shared.footPoseVelocity[side];

        if (footPose.isActive != XR_TRUE) {
            Log::print<INFO>("[LegTrack] foot {} INACTIVE (no tracker bound via {})", sideName, backendLabel);
            continue;
        }
        if ((footLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0) {
            Log::print<INFO>("[LegTrack] foot {} active but position INVALID (flags={:#x})", sideName, (uint32_t)footLocation.locationFlags);
            continue;
        }

        const XrVector3f& p = footLocation.pose.position;
        const XrVector3f& v = footVelocity.linearVelocity;
        const float horizontalSpeed = std::sqrt(v.x * v.x + v.z * v.z);
        Log::print<INFO>("[LegTrack] foot {} pos=({:.3f}, {:.3f}, {:.3f}) hSpeed={:.3f} m/s vY={:.3f}",
            sideName, p.x, p.y, p.z, horizontalSpeed, v.y);
    }
}

void CheckButtonState(bool buttonPressed, ButtonState& buttonState) {
    buttonState.resetFrameFlags();
    
    constexpr std::chrono::milliseconds longPressThreshold{ 250 };
    const bool down = buttonPressed;
    const auto now = std::chrono::steady_clock::now();
    
    // Rising edge - button just pressed
    if (down && !buttonState.wasDownLastFrame) {
        buttonState.pressStartTime = now;
    }
    
    // Pressed state - check for long press threshold
    if (down) {
        auto pressDuration = now - buttonState.pressStartTime;
        
        if (pressDuration >= longPressThreshold) {
            buttonState.lastEvent = ButtonState::Event::LongPress;
            buttonState.longFired = true;
            if (!buttonState.longFired_stillPressed) {
                buttonState.longFired_stillPressed = true;
                buttonState.longFired_actedUpon = true;
            }
        }
    }
    else {
        buttonState.longFired_stillPressed = false;
        buttonState.longFired_actedUpon = false;
    }
    
    // Falling edge - button just released
    if (!down && buttonState.wasDownLastFrame) {
        // Only register short press if long press didn't fire
        if (!buttonState.longFired) {
            buttonState.lastEvent = ButtonState::Event::ShortPress;
        }
        else
            buttonState.longFired = false; // reset long press fired flag
    }
    
    // Store current state for next frame
    buttonState.wasDownLastFrame = down;
}

std::optional<OpenXR::InputState> OpenXR::UpdateActions(XrTime predictedFrameTime, glm::fquat controllerRotation, bool inMenu) {
    BetterVRProfiler::Scope profile(BetterVRProfiler::Section::XRUpdateActions);

    XrActiveActionSet activeActionSet = { (inMenu ? m_menuActionSet : m_gameplayActionSet), XR_NULL_PATH };

    XrActionsSyncInfo syncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    checkXRResult(xrSyncActions(m_session, &syncInfo), "Failed to sync actions!");

    InputState newState = m_input.load();
    newState.shared.in_game = !inMenu;
    newState.shared.inputTime = predictedFrameTime;

    // Locates a pose action inside m_stageSpace. The subaction path is passed in explicitly so that
    // this single helper can serve both the hands (/user/hand/...) and the foot trackers
    // (/user/vive_tracker_htcx/role/...) without duplicating any of the velocity fix-up logic.
    auto locatePose = [&](XrAction action, XrSpace poseSpace, XrPath subactionPath, XrActionStatePose& poseState, XrSpaceLocation& outLocation, XrSpaceVelocity* outVelocity, const char* errorContext) {
        XrActionStateGetInfo getPoseInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getPoseInfo.action = action;
        getPoseInfo.subactionPath = subactionPath;
        poseState = { XR_TYPE_ACTION_STATE_POSE };
        checkXRResult(xrGetActionStatePose(m_session, &getPoseInfo, &poseState), errorContext);

        outLocation = { XR_TYPE_SPACE_LOCATION };
        if (!poseState.isActive) {
            if (outVelocity != nullptr) {
                *outVelocity = { XR_TYPE_SPACE_VELOCITY };
            }
            return;
        }

        XrSpaceLocation spaceLocation = { XR_TYPE_SPACE_LOCATION };
        XrSpaceVelocity spaceVelocity = { XR_TYPE_SPACE_VELOCITY };
        if (outVelocity != nullptr) {
            spaceLocation.next = &spaceVelocity;
            outVelocity->linearVelocity = { 0.0f, 0.0f, 0.0f };
            outVelocity->angularVelocity = { 0.0f, 0.0f, 0.0f };
        }

        checkXRResult(xrLocateSpace(poseSpace, m_stageSpace, predictedFrameTime, &spaceLocation), "Failed to get location from controllers!");
        if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 && (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            outLocation = spaceLocation;

            if (outVelocity != nullptr && (spaceLocation.locationFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) != 0 && (spaceLocation.locationFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) != 0) {
                // rotate angular velocity to world space when it's using a buggy runtime
                auto mode = GetSettings().AngularVelocityFixer_GetMode();
                bool isUsingQuestRuntime = m_capabilities.isOculusLinkRuntime;
                if ((mode == AngularVelocityFixerMode::AUTO && isUsingQuestRuntime) || mode == AngularVelocityFixerMode::FORCED_ON) {
                    glm::vec3 angularVelocity = ToGLM(spaceVelocity.angularVelocity);
                    glm::fquat fix_angle = glm::fquat(0.924, -0.383, 0, 0);
                    angularVelocity = (ToGLM(spaceLocation.pose.orientation) * (fix_angle * angularVelocity)); // TODO: Contact other modders for similar issues with angular velocity being not on the grip rotation (quest 2) + Tune the angular velocity based on manually calculated on rotation positions
                    spaceVelocity.angularVelocity = { angularVelocity.x, angularVelocity.y, angularVelocity.z };
                }

                *outVelocity = spaceVelocity;
            }
        }
    };

    for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
        locatePose(
            newState.shared.in_game ? m_inGameGripPoseAction : m_inMenuGripPoseAction,
            newState.shared.in_game ? m_inGameHandSpaces[side] : m_inMenuHandSpaces[side],
            m_handPaths[side],
            newState.shared.pose[side],
            newState.shared.poseLocation[side],
            &newState.shared.poseVelocity[side],
            "Failed to get pose of controller!"
        );

        locatePose(
            newState.shared.in_game ? m_inGameAimPoseAction : m_inMenuAimPoseAction,
            newState.shared.in_game ? m_inGameAimSpaces[side] : m_inMenuAimSpaces[side],
            m_handPaths[side],
            newState.shared.aimPose[side],
            newState.shared.aimPoseLocation[side],
            nullptr,
            "Failed to get aim pose of controller!"
        );
    }

    // === Leg tracking: dual backend dispatch (XR_HTCX -> OpenVR fallback) ===
    // Both backends write into newState.shared.footPose/footPoseLocation/footPoseVelocity so the
    // downstream consumer (leg motion analyser) doesn't have to know which backend produced the
    // pose. AUTO mode holds XR_HTCX as preferred for up to 10 s of zero-bound foot data, then
    // switches permanently to OpenVR (the SteamVR OpenXR bridge is known to drop ALVR's fake Vive
    // trackers; see XR_ERROR_PATH_UNSUPPORTED in EnsureViveTrackerBindingsSuggested). Manual mode
    // (XR_HTCX only / OpenVR only / Disabled) honours the user choice.
    if (const auto backend = GetSettings().legTrackingBackend.load(); backend != LegTrackingBackend::DISABLED) {
        // (backend is now in scope; reference below)
        const bool inGame = newState.shared.in_game;
        const auto clock = std::chrono::steady_clock::now();

        // Decide the active backend (idempotent — once AUTO decides, it sticks).
        // Cast to int32_t for the switch: MSVC (C2451) rejects `switch (LegTrackingBackend)` because
        // the enum class doesn't implicitly convert to bool (we accidentally wrote it as a ternary
        // condition). The underlying type is int32_t, so static_cast is safe.
        const int32_t backend_i32 = static_cast<int32_t>(backend);
        switch (backend_i32) {
            case static_cast<int32_t>(LegTrackingBackend::XR_HTCX):
                m_activeFootBackend = LegBackend::XrHtcx;
                break;
            case static_cast<int32_t>(LegTrackingBackend::OPENVr):
                m_activeFootBackend = LegBackend::OpenVr;
                break;
            case static_cast<int32_t>(LegTrackingBackend::AUTO):
                if (!m_autoBackendDecided) {
                    if (m_autoBackendFirstFootCheck == std::chrono::steady_clock::time_point{}) {
                        m_autoBackendFirstFootCheck = clock;
                    }
                    // Only flip if XR_HTCX-side binding actually got rejected (PATH_UNSUPPORTED)
                    // OR if 10 s of polling yield zero active trackers. Both conditions imply the
                    // bridge will never surface ALVR's fake Vive trackers to OpenXR this session.
                    const bool xrHtcxRejected = m_footTrackerBindingsSuggested
                        && (m_footPoseAction == XR_NULL_HANDLE || !m_capabilities.supportsViveTracker);
                    const bool tenSecondsNoData = (clock - m_autoBackendFirstFootCheck) > std::chrono::seconds(10);
                    if (xrHtcxRejected || tenSecondsNoData) {
                        m_activeFootBackend = LegBackend::OpenVr;
                        m_autoBackendDecided = true;
                        Log::print<INFO>("[LegTrack] AUTO: switching to OpenVR backend (xrHtcxRejected={}, tenSecondsNoData={})",
                            xrHtcxRejected, tenSecondsNoData);
                    }
                }
                break;
            default:
                m_activeFootBackend = LegBackend::XrHtcx;
                break;
        }

    // Per-backend pose writes. Both produce the same XrSpaceLocation / XrSpaceVelocity layout;
    // whichever ran last wins. Out-of-game clearing is unconditional so stale OpenXR pose data
    // doesn't leak into UI / title-screen motion.
    //
    // OpenVR polling runs even when out-of-game: TryInitialize / device scanning are lazy and
    // only start on the first PollFeet() call, so gating them on in_game would leave the backend
    // dormant (and undiagnosable) on the title screen. Only the pose WRITES stay gated.
    if (m_activeFootBackend == LegBackend::OpenVr && m_openvrClient) {
        const auto feet = m_openvrClient->PollFeet();
        for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
            const OpenVRTrackerPose& p = feet[(int)side];
            if (inGame && p.valid) {
                WriteFootFromOpenVR(side, p, newState);
            } else {
                // Out-of-game, or PollFeet reported valid==false; clear stale data so the
                // consumer can ignore it.
                newState.shared.footPose[side] = { XR_TYPE_ACTION_STATE_POSE };
                newState.shared.footPoseLocation[side] = { XR_TYPE_SPACE_LOCATION };
                newState.shared.footPoseVelocity[side] = { XR_TYPE_SPACE_VELOCITY };
            }
        }
    } else if (!inGame) {
        for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
            newState.shared.footPose[side] = { XR_TYPE_ACTION_STATE_POSE };
            newState.shared.footPoseLocation[side] = { XR_TYPE_SPACE_LOCATION };
            newState.shared.footPoseVelocity[side] = { XR_TYPE_SPACE_VELOCITY };
        }
    } else if (m_activeFootBackend == LegBackend::XrHtcx && m_capabilities.supportsViveTracker && m_footPoseAction != XR_NULL_HANDLE) {
        for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
            locatePose(
                m_footPoseAction,
                m_footSpaces[side],
                m_footPaths[side],
                newState.shared.footPose[side],
                newState.shared.footPoseLocation[side],
                &newState.shared.footPoseVelocity[side],
                "Failed to get pose of foot tracker!"
            );
        }
        // XR_HTCX runtime enumeration + re-bind live in LogFootDiagnostics (1 Hz gated) to
        // avoid polluting the log when the runtime never surfaces trackers.
    }

    LogFootDiagnostics(newState);
} else {
    // Disabled: record the state and clear any stale pose data so the consumer doesn't see
    // a "frozen" frame from the previous backend.
    m_activeFootBackend = LegBackend::Disabled;
    for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
        newState.shared.footPose[side] = { XR_TYPE_ACTION_STATE_POSE };
        newState.shared.footPoseLocation[side] = { XR_TYPE_SPACE_LOCATION };
        newState.shared.footPoseVelocity[side] = { XR_TYPE_SPACE_VELOCITY };
    }
}
    // update shared actions
    XrActionStateGetInfo getInventoryMapInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
    getInventoryMapInfo.action = newState.shared.in_game ? m_inGame_inventory_mapAction : m_inMenu_inventory_mapAction;
    getInventoryMapInfo.subactionPath = XR_NULL_PATH;
    auto& inventory_mapAction = newState.shared.inventory_map;
    inventory_mapAction = { XR_TYPE_ACTION_STATE_BOOLEAN };
    checkXRResult(xrGetActionStateBoolean(m_session, &getInventoryMapInfo, &inventory_mapAction), "Failed to get inventory_help action value!");

    auto& inventory_mapButtonState = newState.shared.inventory_mapState;
    if (inventory_mapAction.isActive == XR_TRUE) {
        auto buttonPressed = inventory_mapAction.currentState == XR_TRUE;
        CheckButtonState(buttonPressed, inventory_mapButtonState); 
    }

    XrActionStateGetInfo getModMenuInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
    getModMenuInfo.action = newState.shared.in_game ? m_inGame_modMenuAction : m_inMenu_modMenuAction;
    getModMenuInfo.subactionPath = XR_NULL_PATH;
    auto& modMenuAction = newState.shared.modMenu;
    modMenuAction = { XR_TYPE_ACTION_STATE_BOOLEAN };
    checkXRResult(xrGetActionStateBoolean(m_session, &getModMenuInfo, &modMenuAction), "Failed to get mod menu action value!");

    auto& modMenuButtonState = newState.shared.modMenuState;
    if (modMenuAction.isActive == XR_TRUE) {
        auto buttonPressed = modMenuAction.currentState == XR_TRUE;
        CheckButtonState(buttonPressed, modMenuButtonState);
    }

    // update in-menu or in-game actions
    if (inMenu) {
        XrActionStateGetInfo getScrollInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getScrollInfo.action = m_scrollAction;
        newState.inMenu.scroll = { XR_TYPE_ACTION_STATE_VECTOR2F };
        checkXRResult(xrGetActionStateVector2f(m_session, &getScrollInfo, &newState.inMenu.scroll), "Failed to get navigate action value!");

        XrActionStateGetInfo getNavigationInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getNavigationInfo.action = m_navigateAction;
        newState.inMenu.navigate = { XR_TYPE_ACTION_STATE_VECTOR2F };
        checkXRResult(xrGetActionStateVector2f(m_session, &getNavigationInfo, &newState.inMenu.navigate), "Failed to get select action value!");

        XrActionStateGetInfo getSelectInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getSelectInfo.action = m_selectAction;
        newState.inMenu.select = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getSelectInfo, &newState.inMenu.select), "Failed to get select action value!");

        XrActionStateGetInfo getBackInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getBackInfo.action = m_backAction;
        newState.inMenu.back = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getBackInfo, &newState.inMenu.back), "Failed to get back action value!");

        XrActionStateGetInfo getSortInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getSortInfo.action = m_sortAction;
        newState.inMenu.sort = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getSortInfo, &newState.inMenu.sort), "Failed to get sort action value!");

        XrActionStateGetInfo getHoldInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getHoldInfo.action = m_holdAction;
        newState.inMenu.hold = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getHoldInfo, &newState.inMenu.hold), "Failed to get hold action value!");

        auto& holdButtonState = newState.inMenu.holdState;
        if (newState.inMenu.hold.isActive == XR_TRUE) {
            auto buttonPressed = newState.inMenu.hold.currentState == XR_TRUE;
            CheckButtonState(buttonPressed, holdButtonState);
        }

        XrActionStateGetInfo getLeftGripInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getLeftGripInfo.action = m_leftGripAction;
        newState.inMenu.leftGrip = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getLeftGripInfo, &newState.inMenu.leftGrip), "Failed to get left grip action value!");

        if (newState.inMenu.leftGrip.currentState == XR_TRUE) {
            newState.shared.lastPickupSide = OpenXR::EyeSide::LEFT;
        }

        XrActionStateGetInfo getRightGripInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getRightGripInfo.action = m_rightGripAction;
        newState.inMenu.rightGrip = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getRightGripInfo, &newState.inMenu.rightGrip), "Failed to get right grip action value!");

        if (newState.inMenu.rightGrip.currentState == XR_TRUE) {
            newState.shared.lastPickupSide = OpenXR::EyeSide::RIGHT;
        }

        XrActionStateGetInfo getLeftTriggerInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getLeftTriggerInfo.action = m_leftTriggerAction;
        getLeftTriggerInfo.subactionPath = XR_NULL_PATH;
        newState.inMenu.leftTrigger = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getLeftTriggerInfo, &newState.inMenu.leftTrigger), "Failed to get left trigger action value!");

        XrActionStateGetInfo getRightTriggerInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getRightTriggerInfo.action = m_rightTriggerAction;
        getRightTriggerInfo.subactionPath = XR_NULL_PATH;
        newState.inMenu.rightTrigger = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getRightTriggerInfo, &newState.inMenu.rightTrigger), "Failed to get right trigger action value!");
    }
    else {
        for (EyeSide side : { EyeSide::LEFT, EyeSide::RIGHT }) {
            XrActionStateGetInfo getGrabInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
            getGrabInfo.action = m_grab_interactAction;
            getGrabInfo.subactionPath = m_handPaths[side];
            newState.inGame.grab[side] = { XR_TYPE_ACTION_STATE_FLOAT };
            checkXRResult(xrGetActionStateFloat(m_session, &getGrabInfo, &newState.inGame.grab[side]), "Failed to get grab action value!");

            auto& buttonState = newState.inGame.grabState[side];
            if (newState.inGame.grab[side].isActive == XR_TRUE) {
                auto buttonPressed = newState.inGame.grab[side].currentState > 0.51f;
                CheckButtonState(buttonPressed, buttonState);
            }
        }

        XrActionStateGetInfo getCrouchScopeInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getCrouchScopeInfo.action = m_crouch_scopeAction;
        getCrouchScopeInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.crouch_scope = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getCrouchScopeInfo, &newState.inGame.crouch_scope), "Failed to get crouch and map action value!");

        auto& crouch_scopeButtonState = newState.inGame.crouch_scopeState;
        if (newState.inGame.crouch_scope.isActive == XR_TRUE) {
            auto buttonPressed = newState.inGame.crouch_scope.currentState == XR_TRUE;
            CheckButtonState(buttonPressed, crouch_scopeButtonState);
        }

        XrActionStateGetInfo getMoveInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getMoveInfo.action = m_moveAction;
        newState.inGame.move = { XR_TYPE_ACTION_STATE_VECTOR2F };
        checkXRResult(xrGetActionStateVector2f(m_session, &getMoveInfo, &newState.inGame.move), "Failed to get move action value!");

        XrActionStateGetInfo getCameraInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getCameraInfo.action = m_cameraAction;
        newState.inGame.camera = { XR_TYPE_ACTION_STATE_VECTOR2F };
        checkXRResult(xrGetActionStateVector2f(m_session, &getCameraInfo, &newState.inGame.camera), "Failed to get camera action value!");

        XrActionStateGetInfo getJumpInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getJumpInfo.action = m_jumpAction;
        getJumpInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.jump_cancel = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getJumpInfo, &newState.inGame.jump_cancel), "Failed to get jump action value!");

        XrActionStateGetInfo getRunInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getRunInfo.action = m_run_interactAction;
        getRunInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.run_interact = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getRunInfo, &newState.inGame.run_interact), "Failed to get run action value!");

        auto& runButtonState = newState.inGame.runState;
        if (newState.inGame.run_interact.isActive == XR_TRUE) {
            auto buttonPressed = newState.inGame.run_interact.currentState == XR_TRUE;
            CheckButtonState(buttonPressed, runButtonState);
        }

        XrActionStateGetInfo getUseRuneInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getUseRuneInfo.action = m_useRune_dpadMenu_Action;
        getUseRuneInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.useRune_dpadMenu = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getUseRuneInfo, &newState.inGame.useRune_dpadMenu), "Failed to get use rune action value!");

        auto& useRuneButtonState = newState.inGame.useRune_runeMenuState;
        if (newState.inGame.useRune_dpadMenu.isActive == XR_TRUE) {
            auto buttonPressed = newState.inGame.useRune_dpadMenu.currentState == XR_TRUE;
            CheckButtonState(buttonPressed, useRuneButtonState);
        }

        XrActionStateGetInfo getUseRightItemInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getUseRightItemInfo.action = m_useRightItemAction;
        getUseRightItemInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.useRightItem = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getUseRightItemInfo, &newState.inGame.useRightItem), "Failed to get useRightItem action value!");

        XrActionStateGetInfo getUseLeftItemInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getUseLeftItemInfo.action = m_useLeftItemAction;
        getUseLeftItemInfo.subactionPath = XR_NULL_PATH;
        newState.inGame.useLeftItem = { XR_TYPE_ACTION_STATE_BOOLEAN };
        checkXRResult(xrGetActionStateBoolean(m_session, &getUseLeftItemInfo, &newState.inGame.useLeftItem), "Failed to get useLeftItem action value!");
    }
    this->m_input.store(newState);
    return newState;
}


std::optional<XrSpaceLocation> OpenXR::UpdateSpaces(XrTime predictedDisplayTime) {
    BetterVRProfiler::Scope profile(BetterVRProfiler::Section::XRUpdateSpaces);

    XrSpaceLocation spaceLocation = { XR_TYPE_SPACE_LOCATION };
    if (XrResult result = xrLocateSpace(m_headSpace, m_stageSpace, predictedDisplayTime, &spaceLocation); XR_SUCCEEDED(result)) {
        if (result != XR_ERROR_TIME_INVALID) {
            checkXRResult(result, "Failed to get space location!");
        }
        checkXRResult(result, "Failed to get space location!");
    }
    if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == 0)
        return std::nullopt;

    return spaceLocation;
}

void OpenXR::ProcessEvents() {
    auto processSessionStateChangedEvent = [this](XrEventDataSessionStateChanged* stateChangedEvent) {
        switch (stateChangedEvent->state) {
            case XR_SESSION_STATE_IDLE:
                Log::print<VERBOSE>("OpenXR has indicated that the session is idle!");
                break;
            case XR_SESSION_STATE_READY: {
                Log::print<VERBOSE>("OpenXR has indicated that the session is ready!");
                if (m_renderer) {
                    Log::print<WARNING>("OpenXR has indicated that the session is ready, but we already have a renderer!");
                }
                else {
                    m_renderer = std::make_unique<RND_Renderer>(m_session);
                }
                break;
            }
            case XR_SESSION_STATE_SYNCHRONIZED:
                Log::print<VERBOSE>("OpenXR has indicated that the session is synchronized!");
                break;
            case XR_SESSION_STATE_FOCUSED:
                Log::print<VERBOSE>("OpenXR has indicated that the session is focused!");
                break;
            case XR_SESSION_STATE_VISIBLE:
                Log::print<VERBOSE>("OpenXR has indicated that the session should be visible!");
                break;
            case XR_SESSION_STATE_STOPPING:
                Log::print<VERBOSE>("OpenXR has indicated that the session should be ended!");
                if (m_renderer) {
                    m_renderer->EndSession();
                }
                break;
            case XR_SESSION_STATE_EXITING:
                Log::print<VERBOSE>("OpenXR has indicated that the session should be destroyed!");
                // an exception is thrown here instead of using exit() to allow Cemu to ideally gracefully shutdown
                //throw std::runtime_error("BetterVR mod has been requested to exit by OpenXR!");
                //this->m_renderer.reset();
                PostMessage(CemuHooks::m_cemuTopWindow, WM_CLOSE, 0, 0);
                break;
            case XR_SESSION_STATE_LOSS_PENDING:
                Log::print<VERBOSE>("OpenXR has indicated that the session is going to be lost!");
                // todo: implement being able to continuously check if xrGetSystem returns and then reinitialize the session
                break;
            default:
                Log::print<VERBOSE>("OpenXR has indicated that an unknown session state has occurred!");
                break;
        }
    };

    XrEventDataBuffer eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    XrResult result = xrPollEvent(m_instance, &eventData);

    while (result == XR_SUCCESS) {
        switch (eventData.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                processSessionStateChangedEvent((XrEventDataSessionStateChanged*)&eventData);
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                Log::print<WARNING>("OpenXR has indicated that the instance is going to be lost!");
                break;
            case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                Log::print<WARNING>("OpenXR has indicated that events are being lost!");
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                Log::print<INFO>("OpenXR has indicated that the interaction profile has changed, re-detecting controller type...");
                m_capabilities.activeControllerType = DetectActiveControllerType(m_instance, m_session);
                break;
            }
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                auto* spaceChangeEvent = (XrEventDataReferenceSpaceChangePending*)&eventData;
                Log::print<WARNING>("OpenXR has indicated that reference space {} has changed (recenter), seated height will be recalibrated!", std::to_underlying(spaceChangeEvent->referenceSpaceType));
                RequestSeatedHeightCalibration(spaceChangeEvent->changeTime);
                break;
            }
            default:
                Log::print<WARNING>("OpenXR has indicated that an unknown event with type {} has occurred!", std::to_underlying(eventData.type));
                break;
        }

        eventData = { XR_TYPE_EVENT_DATA_BUFFER };
        result = xrPollEvent(m_instance, &eventData);
    }
}
