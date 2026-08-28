@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
set VULKAN_SDK=C:\VulkanSDK\1.4.357.0
cd /d C:\Users\win\Downloads\BotW-BetterVR-0.9.22
call build_mod.bat Release
