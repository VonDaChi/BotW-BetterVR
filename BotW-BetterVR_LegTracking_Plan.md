# BotW-BetterVR 腿部追踪（Leg Tracking）支持开发方案

> 目标：在现有 BotW-BetterVR VR Mod 基础上，利用 VR 套件的腿部追踪组件（如 Pico Motion Tracker / Vive Tracker / SlimeVR 等），通过体感新增 **走动、奔跑、跳跃、下蹲** 四项控制。
> 设备前提：Pico Neo 3（一体机串流到 PC 跑 Cemu）+ 腿部 tracer 套件，已验证手柄可正常游玩。
> 方案原则：**最小侵入、全复用现有 VPAD 注入通道**，不重写移动系统。

---

## 0. 可行性结论

**可行。** 仓库现有输入系统已经把 OpenXR 位姿数据汇聚到一个统一的 `InputState` 结构，再在 `controls.cpp` 里转成 Wii U 的 `VPADStatus`（按键 + 左右摇杆）。四项控制全部能挂到已有的 VPAD 注入点上，无需改动游戏补丁（`.asm`）或角色位置补丁。

关键判断：roomscale 只做"碰撞体跟随头显位置"（`entity_controller.cpp` 全是 raycast），**跨地图的真实移动仍靠左摇杆**。因此"腿部走动"应注入到左摇杆移动向量（不是改 roomscale）；"跑/跳/蹲"则置对应的 VPAD 按键。

---

## 1. 源码架构与改动点（已逐行核实）

### 1.1 输入数据流向
```
OpenXR 动作(actions)  ──CreateActions()──▶  m_footPoseAction / m_handPaths
        │
        ▼  xrLocateSpace(..., m_stageSpace, ...)  每帧
InputState.shared.poseLocation[] / poseVelocity[]   (openxr.h)
        │
        ▼  OpenXR::UpdateActions() 把位姿写进 m_input (atomic)
controls.cpp :: CemuHooks::hook_InjectXRInput()
        │  - 读 m_input
        │  - 计算 leftStick / rightStick / 各按键
        ▼
vpadStatus (VPADStatus)  ──writeMemory──▶  游戏读取为手柄输入
```

### 1.2 必须改动 / 参考的文件清单

| 文件 | 作用 | 关键行 |
|---|---|---|
| `src/rendering/openxr.h` | `InputState`、`Shared` 结构定义；能力枚举 | `Shared` 在 60–76 行；`poseLocation`/`poseVelocity` 为 `std::array<…,2>`（65–67）；`Capabilities` 在 18–31 行 |
| `src/rendering/openxr.cpp` | 扩展枚举(33–83)、`CreateActions()`(252–386)、`UpdateActions()`(432–)、`xrLocateSpace`(470) | 见下 |
| `src/utils/controller_bindings.h` | `SuggestControllerBindings()` 把动作绑定到各交互配置 | `CreateActions` 内第 350 行调用 |
| `src/hooking/controls.cpp` | `hook_InjectXRInput()` 注入点 | 左摇杆 941；跳 1144；蹲 1163；跑 1190 |
| `src/hooking/openxr_motion_bridge.h` | 姿态→WiiU 运动数据转换（可参考其坐标变换） | 全文 190 行 |
| `include/game_structs.h` | `VPADStatus`、`VPAD_BUTTON_*`、`VPAD_STICK_L_EMULATION_*` 定义 | — |
| `src/utils/mod_settings.h` + `src/hooking/settings.cpp` | 设置项 | — |
| `src/hooking/imgui_menus.cpp` | BetterVR 菜单 UI（X 键左手呼出） | — |

### 1.3 现有移动/动作注入点（精确）
- **左摇杆移动**：`controls.cpp:941`
  ```cpp
  vpadStatus.leftStick = { leftStickSource.currentState.x + vpadStatus.leftStick.x.getLE(),
                           leftStickSource.currentState.y + vpadStatus.leftStick.y.getLE() };
  ```
  随后 945–953 行把它转成 `VPAD_STICK_L_EMULATION_LEFT/RIGHT/UP/DOWN` 标志。
  `leftStickSource` 来自 `inputs.inGame.move`（`controls.cpp:1086`）。
- **跳跃**：`controls.cpp:1144`
  ```cpp
  newXRBtnHold |= mapXRButtonToVpad(inputs.inGame.jump_cancel, VPAD_BUTTON_X);
  ```
- **奔跑**：`controls.cpp:1189–1191`
  ```cpp
  if (inputs.inGame.runState.lastEvent == ButtonState::Event::LongPress)
      newXRBtnHold |= VPAD_BUTTON_B; // Run
  ```
- **下蹲**：`controls.cpp:1162–1164`
  ```cpp
  if (inputs.inGame.crouch_scopeState.lastEvent == ButtonState::Event::ShortPress)
      newXRBtnHold |= VPAD_BUTTON_STICK_L; // 左摇杆按下 = 蹲
  ```
- **头显位姿可用**：`renderer->GetMiddlePose()`（`controls.cpp:1099`），另有 `shared.hmdRelativePoseLocation`（openxr.h:70）。

---

## 2. 总体实施方案（5 步）

### Step 1 — 启用足部追踪扩展 + 创建足部动作

**(a) 扩展枚举**（`openxr.cpp` 第 33–83 行的 `for` 循环内，仿照 `XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME` 的写法）：
```cpp
bool supportsViveTracker = false;
// 在枚举循环里：
else if (strcmp(extensionProperties.extensionName, XR_HTCX_vive_tracker_interaction_EXTENSION_NAME) == 0) {
    supportsViveTracker = true;
}
// 循环后：
if (supportsViveTracker) enabledExtensions.emplace_back(XR_HTCX_vive_tracker_interaction_EXTENSION_NAME);
```
> 常量 `XR_HTCX_vive_tracker_interaction_EXTENSION_NAME` = `"XR_HTCX_vive_tracker_interaction"`，需要 OpenXR SDK 头文件（项目已依赖 `openxr_loader`）。

**(b) 新增成员**（`openxr.h` 的 `OpenXR` 类内）：
```cpp
XrAction m_footPoseAction = XR_NULL_HANDLE;
std::array<XrPath, 2> m_footPaths{};
std::array<XrSpace, 2> m_footSpaces{};
bool m_supportsViveTracker = false;
```

**(c) 创建动作**（`CreateActions()` 内，在 gameplay action set 创建块中新增）：
```cpp
// HTC Vive Tracker 交互扩展的子路径（注意：不是 /user/foot/left）
m_footPaths = { GetXRPath("/user/vive_tracker_htcx/role/left_foot"),
                GetXRPath("/user/vive_tracker_htcx/role/right_foot") };
createAction(m_gameplayActionSet, "foot_pose", "Foot Pose", XR_ACTION_TYPE_POSE_INPUT, m_footPoseAction);
```
> 注意：`createAction` 当前的 `subactionPaths` 默认是 `m_handPaths`。为足部动作单独创建动作时，需让它的 `countSubactionPaths`/`subactionPaths` 指向 `m_footPaths`。可新增一个 `createActionEx` 重载，或在调用后手动覆盖 `actionInfo.subactionPaths`。

**(d) 创建 action space + 绑定**（`CreateActions()` 末尾循环，仿照第 359–368 行）：
```cpp
for (int i = 0; i < 2; i++) {
    XrActionSpaceCreateInfo ci = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
    ci.action = m_footPoseAction;
    ci.subactionPath = m_footPaths[i];
    ci.poseInActionSpace = s_xrIdentityPose;
    checkXRResult(xrCreateActionSpace(m_session, &ci, &m_footSpaces[i]), "Failed to create foot action space!");
}
```
在 `controller_bindings.h` 的 `SuggestControllerBindings()` 内，把 `m_footPoseAction` 绑定到 HTCX Vive Tracker 交互配置：
- 交互配置路径：`/interaction_profiles/htcx/vive_tracker_htcx`
- 组件路径：`/user/vive_tracker_htcx/role/left_foot/input/grip/pose` 与 `/user/vive_tracker_htcx/role/right_foot/input/grip/pose`

**(e) 能力位**：在 `Capabilities` 结构（openxr.h:18）加 `bool supportsViveTracker = false;`，并在 `CreateActions` 中探测后赋值，供设置项/诊断使用。

### Step 2 — 每帧定位足部位姿

- 在 `openxr.h` 的 `Shared` 结构里新增：
  ```cpp
  std::array<XrSpaceLocation, 2> footPoseLocation{};
  std::array<XrSpaceVelocity, 2> footPoseVelocity{};
  std::array<XrActionStatePose, 2> footPose{};
  ```
- 在 `UpdateActions()`（第 446–507 行的手部 `locatePose` 循环之后），**复用已有的 `locatePose` lambda**（它已处理 `xrGetActionStatePose` + `xrLocateSpace(m_stageSpace)` + 速度），对左右脚各调用一次：
  ```cpp
  for (int i = 0; i < 2; i++) {
      locatePose(m_footPoseAction, m_footSpaces[i],
                 newState.shared.footPose[i],
                 newState.shared.footPoseLocation[i],
                 &newState.shared.footPoseVelocity[i],
                 "Failed to get foot pose!");
  }
  ```
- `isActive == false`（未绑定/未佩戴）时，`locatePose` 已能优雅跳过，无需额外处理。

### Step 3 — 步态分析器（核心算法）

新建 `src/hooking/leg_motion.h` + `leg_motion.cpp`：
```cpp
struct LegMotionResult {
    glm::vec2 walkVector;   // x=右, y=前 (相对玩家朝向)，范围约 [-1,1]
    bool      isJump;       // 边沿触发（一帧）
    bool      isRun;        // 持续态
    bool      isCrouch;     // 持续态
};

class LegMotionAnalyzer {
public:
    void Reset();
    LegMotionResult Update(
        const std::array<XrSpaceLocation,2>& footLoc,
        const std::array<XrSpaceVelocity,2>& footVel,
        const glm::vec3& hmdPos,        // 世界坐标，Y-up
        const glm::quat& hmdRot,        // 用于取朝向
        float dt,
        const LegTrackingSettings& cfg); // 灵敏度/阈值
    void Calibrate(const glm::vec3& hmdPos, const std::array<XrSpaceLocation,2>& footLoc);
private:
    // 滤波状态、标定基准、上一次脚位、跳跃/下蹲边沿与冷却计时
};
```
算法细节见第 3 节。

### Step 4 — 注入 controls.cpp

在 `hook_InjectXRInput()` 内、读取 `leftStickSource` 之后（约 `controls.cpp:941` 之前）调用分析器，拿到 `LegMotionResult leg`：

- **走动**（叠加进左摇杆）：
  ```cpp
  if (cfg.legTrackingEnabled && footValid) {
      leftStickSource.currentState.x += leg.walkVector.x;
      leftStickSource.currentState.y += leg.walkVector.y;
  }
  ```
  之后 941–953 行的死区/仿真标志逻辑会自动把它转成 `VPAD_STICK_L_EMULATION_*`，无需改动。

- **奔跑**：在第 1189 行附近，
  ```cpp
  if (leg.isRun) newXRBtnHold |= VPAD_BUTTON_B;
  ```
- **跳跃**：在第 1144 行附近，
  ```cpp
  if (leg.isJump) newXRBtnHold |= VPAD_BUTTON_X;
  ```
- **下蹲**：在第 1162 行附近，
  ```cpp
  if (leg.isCrouch) newXRBtnHold |= VPAD_BUTTON_STICK_L; // 持续按住=蹲
  ```

> 注意：`leftStickSource` 在 1086 行取自 `inputs.inGame.move`，死区在 1090–1096 应用。腿部向量建议在**死区之后、941 之前**叠加，避免被死区吞掉；或直接修改死区逻辑把 leg 向量按配置的最低灵敏度放行。

### Step 5 — 设置项 / 标定 / UI

- `mod_settings.h` 新增：
  ```cpp
  struct LegTrackingSettings {
      bool   enabled = false;
      float  walkSensitivity = 1.0f;   // 脚速→移动向量的增益
      float  runSpeedThreshold = 1.6f;  // m/s，超过判奔跑
      float  crouchDepth = 0.15f;       // m，头显低于站立高度多少判蹲
      float  jumpLiftThreshold = 0.08f; // m，双脚抬升触发跳跃
      float  deadzone = 0.15f;          // 腿部移动死区
      bool   calibrated = false;
  };
  ```
- `settings.cpp` 序列化/反序列化（与现有设置同机制）。
- `imgui_menus.cpp` 在 BetterVR 菜单加"Leg Tracking"分组：开关、各滑块、"Calibrate（标定）"按钮、实时诊断（脚部位姿有效性/速度）。

---

## 3. 四项控制的具体算法

坐标系：所有位姿在 `m_stageSpace`（房间坐标系，Y-up，单位米）。`hmdPos` 取头显世界坐标。

### 3.1 走动（walkVector）
1. 取双脚水平速度：`vL = footVel[0].linearVelocity`, `vR = footVel[1].linearVelocity`，只取 x/z。
2. 平均脚速 `vAvg = (vL + vR) / 2`（水平）。
3. 行走时脚相对身体向后蹬 → 身体前进方向 ≈ 脚运动的反方向。故身体意图速度 `bodyIntent = -vAvg`（水平）。
4. 把 `bodyIntent` 旋转到**玩家朝向**（用 `hmdRot` 的 yaw）：得到相对前/右的 `walkVector`。
5. 增益与死区：`walkVector = clamp(bodyIntent * walkSensitivity, -1, 1)`，低于 `deadzone` 归零。
6. 低通滤波（指数滑动平均）消除抖动。

> 说明：你已在房间里真实走动时，roomscale 已让碰撞体跟随头显移动（原地挪动没问题）。腿追功能补的是**原地踏步/原地跑**驱动长距离移动。

### 3.2 奔跑（isRun）
- 当 `|vAvg|`（脚水平速度幅值）持续超过 `runSpeedThreshold` → `isRun = true`。
- 或检测到更大幅度的蹬地（步幅/步频更高）。触发时置 `VPAD_BUTTON_B`。

### 3.3 跳跃（isJump，边沿触发）
- 检测"双脚快速离地"：任一脚（或双脚）Y 高度在短窗口内抬升超过 `jumpLiftThreshold`，随后回落。
- 用状态机：检测上升沿 → 置 `isJump` 一帧 → 进入冷却（如 400ms）防止连跳。
- 备选：检测双脚突然向上的加速度（`footVel.linearVelocity.y` 峰值）。

### 3.4 下蹲（isCrouch，持续态）
- 脚基本留在地面（脚 Y 接近标定值），但**头显高度** `hmdPos.y` 低于"站立标定高度 − crouchDepth" → 判定屈膝下蹲。
- 用 HMD 高度判定最稳（不依赖脚 tracker 的姿态精度）。
- 持续按住 `VPAD_BUTTON_STICK_L`（BotW 中按住左摇杆按下=蹲，松开=站）。

---

## 4. 标定流程

1. 玩家站立不动 → 按菜单"Calibrate"。
2. 记录：`standHmdY = hmdPos.y`；`standFootY = 双脚平均 Y`；`baseFootSpeed`（静止噪声基线）。
3. 存盘（`calibrated = true`）。
4. 运行时所有高度/速度判定都以标定值为基准，避免不同身高、不同佩戴高度导致误判。

---

## 5. 最大风险 & 验证顺序（务必先看这条）

### ★ 实测状态（2026-08-23 用户已验证）
- **设备/串流配置**：Pico Neo 3 + 腿部 tracer 套件，经 **ALVR + SteamVR** 串流到 PC（非 PICO Connect）。
- **关键观查**：在 ALVR 的 Headset 选项卡 → Haptics → **Emulation mode 选 "PICO4"** 后，**SteamVR 设置里出现一系列 tracker 角色选项，包含 Hand 与 Foot**。
- **结论**：这说明 Pico Motion Tracker 在 PC 端**已作为可指派角色的 SteamVR Tracker 设备暴露**（ALVR 的 PICO4 emulation 把 Pico tracker 映射成 SteamVR 兼容的 Vive Tracker 角色）。→ **本方案主路径（Path 1：`XR_HTCX_vive_tracker_interaction`）已实证可行**，无需走骨骼关节备选方案。
- **仍需在代码里最终确认的一件事**：把两个踝部 tracker 在 SteamVR 里指派为 **Left Foot / Right Foot** 角色后，BetterVR 能否通过 `xrEnumerateBoundSourcesForAction` 拿到 `localizedName == "Left Foot"/"Right Foot"` 且 `footPose[i].isActive == true`。（即方案 Step 1–2 的诊断，见下。）

### 头号风险（已基本排除，保留作备份参考）
Pico 腿追在 PC OpenXR 里是否以 foot 角色暴露——**此风险已通过用户实测排除**（见上）。下方为原分析，仅供排查特殊情况时参考：
- Pico Motion Tracker 用 2.4GHz 私有协议与 **Pico 头显**配对，全身体态是头显端 AI 算出来的；PC 端靠 **ALVR / PICO Connect** 串流，官方称兼容"SteamVR 体感游戏"。两种可能的 PC 暴露方式：
  1. **作为 SteamVR Tracker 设备暴露（已确认走通）**：tracker 作为独立 `Tracker` 设备出现在 SteamVR，可在"管理 Vive Tracker"里指派 Left Foot / Right Foot 角色 → 经过 `XR_HTCX_vive_tracker_interaction` 暴露为 `/user/vive_tracker_htcx/role/left_foot` / `right_foot`。**本方案按此路径设计，且已实证。**
  2. **仅作为骨骼/关节暴露（当前非首选，仅作后备）**：若将来某种串流方式只把全身骨架（hips/leftFoot/rightFoot 等关节）通过 OpenXR 身体追踪扩展（`XR_FB_body_tracking` / `XR_EXT_body_tracking`）送出、**不出现独立 tracker 设备**，则需走"读关节"的另类集成（直接用 `XrBodyJointLocation` 取脚部位姿），与本方案动作/绑定方式不同。
- **关于串流软件**：实测确认 **ALVR + PICO4 emulation** 能转发足部 tracker 到 SteamVR；若改用其他串流（Virtual Desktop / Pico Link），仍建议把活动 OpenXR runtime 设为 **SteamVR** 以保证 `XR_HTCX_vive_tracker_interaction` 可用。

### 建议实施顺序（降低返工）
1. **先做诊断（Step 1–2 的精简版）**：只启用扩展 + 创建足部动作 + 每帧定位，把 `footPoseLocation` 的 `isActive`、坐标、速度打到日志 / BetterVR 调试覆盖层。**先确认腿追数据能读到**，再往下写算法。
2. 诊断通过后再写 Step 3 算法 + Step 4 注入。
3. 最后做 Step 5 设置/标定/UI。

### 其他注意事项
- `xrCreateActionSpace` 要求动作已绑定；若 foot 路径不存在，`poseState.isActive == false`，现有 `locatePose` 已能跳过，不会崩。
- 需确认项目 OpenXR 头文件版本包含 `XR_HTCX_vive_tracker_interaction`（vcpkg 依赖 `openxr-loader`，必要时升级）。
- 所有新增设置需有默认值且**默认关闭**，避免影响现有手柄玩家。
- 建议加一个"腿部控制开关 + 调试浮层"，实时显示脚部位姿/速度/判定结果，便于调参。

---

## 6. 验收清单（完成后自测）

- [ ] 菜单能识别 `supportsViveTracker` 并打印日志。
- [ ] 诊断浮层显示左右脚 `isActive=true`、合理坐标与速度。
- [ ] 原地踏步 → 角色朝面朝方向稳定移动；停下 → 停止。
- [ ] 加快踏步/大步 → 进入奔跑（移动更快 + 奔跑键生效）。
- [ ] 双脚快速离地 → 触发一次跳跃（有冷却，不连跳）。
- [ ] 屈膝下蹲 → 角色下蹲；站直 → 站起。
- [ ] 关闭腿部开关后，原有手柄控制完全不受影响。
- [ ] 标定后不同身高/佩戴高度下判定稳定。

---

## 附：关键原始代码片段（便于直接对照）

`openxr.h` 现有 `Shared`（改动参考）：
```cpp
struct Shared {
    bool in_game = true;
    XrTime inputTime;
    std::optional<EyeSide> lastPickupSide = std::nullopt;
    std::array<XrActionStatePose, 2> pose;
    std::array<XrSpaceLocation, 2> poseLocation;
    std::array<XrSpaceVelocity, 2> poseVelocity;
    // ...（新增 footPose / footPoseLocation / footPoseVelocity）
};
```

`openxr.cpp` 现有 `locatePose` lambda（Step 2 直接复用，无需重写）：
```cpp
auto locatePose = [&](XrAction action, XrSpace handSpace, XrActionStatePose& poseState,
                      XrSpaceLocation& outLocation, XrSpaceVelocity* outVelocity, const char* errorContext) {
    // ... xrGetActionStatePose + xrLocateSpace(m_stageSpace) + 速度有效性处理
};
```

`controls.cpp` 现有移动注入（Step 4 在其前叠加 leg 向量）：
```cpp
vpadStatus.leftStick = { leftStickSource.currentState.x + ..., leftStickSource.currentState.y + ... };
```

---
*本方案基于 Crementif/BotW-BetterVR 仓库 `main` 分支源码分析（2026-08 核查）。实际实现时请以当时最新代码为准，行号可能随版本变动。*
