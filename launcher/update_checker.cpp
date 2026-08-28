#include "pch.h"

#include "launcher_common.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")
#include <winhttp.h>

namespace UpdateChecker {
    std::string TrimPrefixV(std::string value) {
        if (!value.empty() && (value[0] == 'v' || value[0] == 'V')) {
            value.erase(value.begin());
        }
        return value;
    }

    std::vector<uint32_t> ParseVersionParts(const std::string& version) {
        std::vector<uint32_t> parts;
        std::string value = TrimPrefixV(version);
        uint64_t accumulator = 0;
        bool inNumber = false;

        for (char character : value) {
            if (character >= '0' && character <= '9') {
                inNumber = true;
                accumulator = accumulator * 10 + static_cast<uint64_t>(character - '0');
            }
            else {
                if (inNumber) {
                    parts.push_back(static_cast<uint32_t>(std::min<uint64_t>(accumulator, UINT32_MAX)));
                    accumulator = 0;
                    inNumber = false;
                }
                if (character != '.') {
                    break;
                }
            }
        }

        if (inNumber) {
            parts.push_back(static_cast<uint32_t>(std::min<uint64_t>(accumulator, UINT32_MAX)));
        }

        while (parts.size() < 3) {
            parts.push_back(0);
        }

        return parts;
    }

    int CompareVersions(const std::string& left, const std::string& right) {
        const auto leftParts = ParseVersionParts(left);
        const auto rightParts = ParseVersionParts(right);
        const size_t count = std::max(leftParts.size(), rightParts.size());

        for (size_t index = 0; index < count; ++index) {
            const uint32_t leftValue = index < leftParts.size() ? leftParts[index] : 0;
            const uint32_t rightValue = index < rightParts.size() ? rightParts[index] : 0;
            if (leftValue < rightValue) {
                return -1;
            }
            if (leftValue > rightValue) {
                return 1;
            }
        }

        return 0;
    }

    std::string LoadIgnoredVersionOnce(const LauncherPaths& paths) {
        std::ifstream file(paths.updateIgnoreFile, std::ios::in);
        if (!file.is_open()) {
            return {};
        }

        std::string ignoredVersion;
        std::getline(file, ignoredVersion);
        return ignoredVersion;
    }

    void SaveIgnoredVersionOnce(const LauncherPaths& paths, const std::string& version) {
        std::ofstream file(paths.updateIgnoreFile, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            return;
        }

        file << version;
    }

    struct CemuVersion {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t build = 0;
        uint32_t revision = 0;
    };

    std::string FormatCemuVersion(const CemuVersion& version) {
        return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.build) + "." + std::to_string(version.revision);
    }

    std::optional<CemuVersion> GetCemuFileVersion(const fs::path& exePath) {
        const DWORD versionInfoSize = GetFileVersionInfoSizeW(exePath.c_str(), nullptr);
        if (versionInfoSize == 0) {
            LogLine("Cemu version check: GetFileVersionInfoSizeW failed for " + Narrow(exePath));
            return std::nullopt;
        }

        std::vector<BYTE> versionInfo(versionInfoSize);
        if (!GetFileVersionInfoW(exePath.c_str(), 0, versionInfoSize, versionInfo.data())) {
            LogLine("Cemu version check: GetFileVersionInfoW failed for " + Narrow(exePath));
            return std::nullopt;
        }

        VS_FIXEDFILEINFO* fixedFileInfo = nullptr;
        UINT fixedFileInfoSize = 0;
        if (!VerQueryValueW(versionInfo.data(), L"\\", reinterpret_cast<LPVOID*>(&fixedFileInfo), &fixedFileInfoSize) || fixedFileInfo == nullptr) {
            LogLine("Cemu version check: VerQueryValueW failed for " + Narrow(exePath));
            return std::nullopt;
        }

        CemuVersion version;
        version.major = HIWORD(fixedFileInfo->dwProductVersionMS);
        version.minor = LOWORD(fixedFileInfo->dwProductVersionMS);
        version.build = HIWORD(fixedFileInfo->dwProductVersionLS);
        version.revision = LOWORD(fixedFileInfo->dwProductVersionLS);
        return version;
    }

    bool IsZeroCemuVersion(const CemuVersion& version) {
        return version.major == 0 && version.minor == 0 && version.build == 0 && version.revision == 0;
    }

    bool CheckCemuVersion(const LauncherPaths& paths) {
        const std::optional<CemuVersion> version = GetCemuFileVersion(paths.cemuExe);
        if (!version.has_value()) {
            LogLine("Cemu version check: could not determine Cemu version, allowing launch");
            return true;
        }

        if (IsZeroCemuVersion(*version)) {
            LogLine("Cemu version check: version is 0.0.0.0 (likely local build), allowing launch");
            return true;
        }

        const std::string versionString = FormatCemuVersion(*version);
        LogLine("Cemu version: " + versionString);

        if (version->major < 2) {
            std::string message;
            message += "You are running Cemu ";
            message += versionString;
            message += " (1.x), which is incompatible with BetterVR.\n\n";
            message += "Please update to Cemu 2.6 or newer.\n";
            message += "It is strongly recommended to set up Cemu 2.6 from scratch:\n";
            message += "https://cemu.cfw.guide/";
            ShowErrorMessage(message);
            return false;
        }

        if (version->major == 2 && version->minor < 4) {
            std::string message;
            message += "You are running Cemu ";
            message += versionString;
            message += ", which is older than Cemu 2.4 and hasn't been tested to work.\n\n";
            message += "It's recommended to update Cemu.\n\n";
            message += "Launch Cemu anyway?";
            const int choice = MessageBoxA(nullptr, message.c_str(), "BetterVR Launcher - Outdated Cemu", MB_YESNO | MB_ICONWARNING);
            if (choice != IDYES) {
                LogLine("Cemu version check: user declined to launch outdated Cemu");
                return false;
            }
        }

        return true;
    }

    void CheckForUpdates(const LauncherPaths& paths) {
#ifndef _DEBUG
        std::thread([paths]() {
            constexpr const char* CurrentVersion = BETTERVR_LAUNCHER_VERSION;

            auto session = WinHttpOpen(L"BotW-BetterVR Update Checker", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (session == nullptr) {
                LogLine("Update check skipped: WinHttpOpen failed");
                return;
            }

            auto connect = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (connect == nullptr) {
                LogLine("Update check skipped: WinHttpConnect failed");
                WinHttpCloseHandle(session);
                return;
            }

            auto request = WinHttpOpenRequest(connect, L"GET", L"/repos/Crementif/BotW-BetterVR/releases/latest", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
            if (request == nullptr) {
                LogLine("Update check skipped: WinHttpOpenRequest failed");
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return;
            }

            const wchar_t* headers = L"User-Agent: BotW-BetterVR-Launcher\r\nAccept: application/vnd.github+json\r\n";
            BOOL success = WinHttpSendRequest(request, headers, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (success == TRUE) {
                success = WinHttpReceiveResponse(request, nullptr);
            }

            if (success == TRUE) {
                std::string response;
                DWORD size = 0;
                do {
                    if (!WinHttpQueryDataAvailable(request, &size)) {
                        LogLine("Update check failed during WinHttpQueryDataAvailable");
                        break;
                    }
                    if (size == 0) {
                        break;
                    }

                    std::vector<char> buffer(size);
                    DWORD downloaded = 0;
                    if (!WinHttpReadData(request, buffer.data(), size, &downloaded)) {
                        LogLine("Update check failed during WinHttpReadData");
                        break;
                    }

                    response.append(buffer.data(), downloaded);
                } while (size > 0);

                const size_t tagNamePos = response.find("\"tag_name\"");
                if (tagNamePos != std::string::npos) {
                    const size_t colonPos = response.find(':', tagNamePos);
                    const size_t valueStart = response.find('"', colonPos);
                    const size_t valueEnd = response.find('"', valueStart + 1);
                    if (colonPos != std::string::npos && valueStart != std::string::npos && valueEnd != std::string::npos) {
                        const std::string latestVersion = response.substr(valueStart + 1, valueEnd - valueStart - 1);
                        if (CompareVersions(CurrentVersion, latestVersion) < 0) {
                            const std::string ignoredVersion = LoadIgnoredVersionOnce(paths);
                            if (ignoredVersion == latestVersion) {
                                LogLine("Update ignored once: " + latestVersion);
                            }
                            else {
                                LogLine("Update available: " + latestVersion);

                                std::string message;
                                message += "A new version of BotW-BetterVR is available.\n\n";
                                message += "Current: ";
                                message += CurrentVersion;
                                message += "\nLatest:  ";
                                message += latestVersion;
                                message += "\n\nOpen releases page?\n\n";
                                message += "Yes = Open page\nNo = Dismiss\nCancel = Ignore this update once";

                                const int choice = MessageBoxA(nullptr, message.c_str(), "BetterVR Update Available", MB_YESNOCANCEL | MB_ICONINFORMATION);
                                if (choice == IDYES) {
                                    ShellExecuteA(nullptr, "open", "https://github.com/Crementif/BotW-BetterVR/releases", nullptr, nullptr, SW_SHOWNORMAL);
                                }
                                else if (choice == IDCANCEL) {
                                    SaveIgnoredVersionOnce(paths, latestVersion);
                                }
                            }
                        }
                    }
                }
            }

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
        }).detach();
#endif
    }
}
