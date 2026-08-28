#include "pch.h"

#include <vector>

#include "launcher_common.h"

static std::string XrVersionToString(XrVersion version) {
    return std::to_string(XR_VERSION_MAJOR(version)) + "." + std::to_string(XR_VERSION_MINOR(version)) + "." + std::to_string(XR_VERSION_PATCH(version));
}

static std::string XrResultToString(XrInstance instance, XrResult result) {
    char buffer[XR_MAX_RESULT_STRING_SIZE] = {};
    if (instance != XR_NULL_HANDLE && XR_SUCCEEDED(xrResultToString(instance, result, buffer))) {
        return buffer;
    }

    switch (result) {
        case XR_ERROR_RUNTIME_FAILURE:
            return "XR_ERROR_RUNTIME_FAILURE";
        case XR_ERROR_RUNTIME_UNAVAILABLE:
            return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE:
            return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNSUPPORTED:
            return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
        case XR_ERROR_INSTANCE_LOST:
            return "XR_ERROR_INSTANCE_LOST";
        case XR_ERROR_SYSTEM_INVALID:
            return "XR_ERROR_SYSTEM_INVALID";
        case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED:
            return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
        default:
            return std::to_string(result);
    }
}

static std::string DescribeOpenXRFailure(const char* operation, XrResult result) {
    std::string message = std::string(operation) + " failed.";

    switch (result) {
        case XR_ERROR_RUNTIME_FAILURE:
            message += " The active OpenXR runtime failed to initialize. Restart the runtime or PC, then confirm the correct runtime is selected in SteamVR, Quest Link, or your headset software.";
            break;
        case XR_ERROR_RUNTIME_UNAVAILABLE:
            message += " No OpenXR runtime is available. Install or enable an OpenXR runtime and set it as active.";
            break;
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE:
            message += " No VR headset is currently available. Make sure the headset is connected, powered on, awake, and recognized by the active OpenXR runtime.";
            break;
        case XR_ERROR_FORM_FACTOR_UNSUPPORTED:
            message += " The active OpenXR runtime does not support head-mounted displays.";
            break;
        case XR_ERROR_INSTANCE_LOST:
            message += " The OpenXR runtime lost its instance. Restart the runtime and try again.";
            break;
        case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED:
            message += " The connected headset/runtime does not support stereo rendering.";
            break;
        default:
            break;
    }

    return message;
}

static void LogOpenXRFailure(XrInstance instance, const char* operation, XrResult result) {
    LogLine(DescribeOpenXRFailure(operation, result) + " Result: " + XrResultToString(instance, result));
}

OpenXRProbeResult ProbeOpenXR() {
    OpenXRProbeResult result{};

    XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
    createInfo.applicationInfo = { "BetterVR", 1, "Cemu", 1, XR_API_VERSION_1_0 };

    XrInstance instance = XR_NULL_HANDLE;
    const XrResult createResult = xrCreateInstance(&createInfo, &instance);
    if (XR_FAILED(createResult)) {
        LogOpenXRFailure(XR_NULL_HANDLE, "OpenXR instance creation", createResult);
        result.errorMessage = DescribeOpenXRFailure("Failed to initialize the OpenXR runtime", createResult);
        return result;
    }

    XrInstanceProperties instanceProperties{ XR_TYPE_INSTANCE_PROPERTIES };
    if (XR_SUCCEEDED(xrGetInstanceProperties(instance, &instanceProperties))) {
        result.runtimeName = instanceProperties.runtimeName;
        result.runtimeVersion = XrVersionToString(instanceProperties.runtimeVersion);
    }

    XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    const XrResult getSystemResult = xrGetSystem(instance, &systemGetInfo, &systemId);
    if (XR_FAILED(getSystemResult)) {
        LogOpenXRFailure(instance, "OpenXR headset detection", getSystemResult);
        result.errorMessage = DescribeOpenXRFailure("No VR headset was available to the OpenXR runtime", getSystemResult);
        xrDestroyInstance(instance);
        return result;
    }

    XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
    const XrResult systemPropertiesResult = xrGetSystemProperties(instance, systemId, &systemProperties);
    if (XR_FAILED(systemPropertiesResult)) {
        LogOpenXRFailure(instance, "OpenXR system property query", systemPropertiesResult);
        result.errorMessage = DescribeOpenXRFailure("Failed to read OpenXR headset properties", systemPropertiesResult);
        xrDestroyInstance(instance);
        return result;
    }
    result.systemName = systemProperties.systemName;

    uint32_t viewCount = 0;
    const XrResult enumerateViewsCountResult = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if (XR_FAILED(enumerateViewsCountResult)) {
        LogOpenXRFailure(instance, "OpenXR stereo view query", enumerateViewsCountResult);
        result.errorMessage = DescribeOpenXRFailure("The connected headset does not provide stereo rendering information", enumerateViewsCountResult);
        xrDestroyInstance(instance);
        return result;
    }

    if (viewCount > 0) {
        std::vector<XrViewConfigurationView> views(viewCount);
        for (XrViewConfigurationView& view : views) {
            view = { XR_TYPE_VIEW_CONFIGURATION_VIEW };
        }

        const XrResult enumerateViewsResult = xrEnumerateViewConfigurationViews(instance, systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, views.data());
        if (XR_FAILED(enumerateViewsResult)) {
            LogOpenXRFailure(instance, "OpenXR stereo view enumeration", enumerateViewsResult);
            result.errorMessage = DescribeOpenXRFailure("Failed to query OpenXR eye resolution information", enumerateViewsResult);
            xrDestroyInstance(instance);
            return result;
        }

        result.leftWidth = views[0].recommendedImageRectWidth;
        result.leftHeight = views[0].recommendedImageRectHeight;
        if (viewCount > 1) {
            result.rightWidth = views[1].recommendedImageRectWidth;
            result.rightHeight = views[1].recommendedImageRectHeight;
        }
    }

    xrDestroyInstance(instance);

    if (!result.runtimeName.empty()) {
        LogLine("OpenXR runtime: " + result.runtimeName + " " + result.runtimeVersion);
    }
    if (!result.systemName.empty()) {
        LogLine("OpenXR headset: " + result.systemName);
    }
    if (result.leftWidth != 0 && result.leftHeight != 0) {
        LogLine("OpenXR eye resolution: " + std::to_string(result.leftWidth) + "x" + std::to_string(result.leftHeight));
    }

    return result;
}
