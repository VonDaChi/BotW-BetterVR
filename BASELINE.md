# BetterVR 腿追改造 · 改动前基线（BASELINE）

> 本文件记录「未做任何腿部追踪改动」前、从源码成功编译出的 BetterVR 版本。
> 作用：之后任何改动若导致崩溃/异常，可与此基线对比（行为、日志、构建差异）。

## 源码状态
- 版本：BotW-BetterVR **0.9.22**（发布包，非 git 仓库）
- 改动：**无**（纯基线）
- 关键预设：`CMakeUserPresets.json` 三处 `VCPKG_ROOT` 已指向 `C:\vcpkg`（原仓库写死 `C:\Programs\vcpkg`，本机不存在）

## 构建（已验证成功）
- 配置：**Release**（Ninja 生成器，来自隐藏 `vcpkg` 预设；见下方「重要修正」）
- 时间：2026-08-24 17:09（约 5 分钟装依赖 + 53s 配置 + 编译安装）
- 结果：**成功，exit=0，无报错**
- 产物（位于 `./Cemu`）：
  - `BetterVR_Launcher.exe`（4.6 MB）
  - `BetterVR_Layer.dll`（3.3 MB）
  - `BetterVR_Layer.direct.json`
  - `graphicPacks/BreathOfTheWild_BetterVR/`（含全套 `*.asm` 与 `rules.txt`）

### ⚠️ 重要修正：本机正确的构建命令（不要直接用 build_mod.bat）
`build_mod.bat` 内部用 `-G "Visual Studio 18 2026"`，在**本沙箱环境**下会因 CMake 调用 MSBuild 探测 `VCTargetsPath` 时**访问冲突（Access violation）**而失败（DevShell 环境未正确加载 + MSBuild 受限）。

**本机可用命令（从 VS2026 x64 开发者环境 + 设置 VULKAN_SDK 后执行）：**
```
# 在 Visual Studio 2026 的 x64 Native Tools 开发者 shell 中：
set VULKAN_SDK=C:\VulkanSDK\1.4.357.0
cmake --preset Release                 # 配置（Ninja，Release）
cmake --build ./cmake-build-Release --target install   # 编译并安装到 ./Cemu
```
- 生成目录：`cmake-build-Release/`（预设自带，非 `./build`）
- 安装目标名是**小写 `install`**（Ninja 不用大写 `INSTALL`）
- 依赖（openxr-loader / glm / vulkan-headers / imgui / implot 等）已编译并缓存，重跑秒过

## 运行（待你人工确认）
- [ ] 启动 `Cemu/BetterVR_Launcher.exe` → Cemu 图形包启用 **BetterVR** + **FPS++**
- [ ] 启动 BotW → 确认 VR 正常（头显 + 双手柄）
- 说明：构建产物已就位，但「自己编译的版本能跑」需你在 Cemu 里实际启动一次验证。

## 硬件 / 串流状态（step 0.3）
- 设备：Pico Neo 3 + 全身 tracers
- 串流：ALVR + PICO4 emulation
- SteamVR：可见手腕/肘/膝/踝全身追踪
- **Foot 角色指派：□ 能  /  □ 不能**（见下方提醒，step 1 前务必确认）

> ⚠️ 决定 step 1 诊断主/备路径：
> - 踝部设备能在 SteamVR「管理 Vive Tracker」中指派为 **Left Foot / Right Foot** → 主路径 `XR_HTCX_vive_tracker_interaction` 可用，诊断预期 `footPose.isActive=true`。
> - 若不能（仅为 AI 骨架可视化、无独立可指派 tracker）→ 诊断时 `isActive` 持续 false，需切 `XR_FB_body_tracking` 读脚关节备选路径。
