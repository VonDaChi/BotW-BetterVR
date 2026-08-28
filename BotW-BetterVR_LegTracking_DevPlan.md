# BotW-BetterVR 腿部追踪 · 渐进式开发计划（非 C/C++ 友好版）

> 适用对象：具备 Python 编程 + 电子 DIY 经验、但**无 C/C++ 基础**的开发者。
> 配套文档：`BotW-BetterVR_LegTracking_Plan.md`（技术原案，含逐行源码分析）。
> 核心思路：**你负责"逻辑 / 脚本 / 数据处理 / 硬件链路验证"，C/C++ 只做"填空式落地"**——
> 即你产出算法、阈值、测试数据，再用原案片段或 AI 协助把相同逻辑翻译成 C++，你只负责核对"行为一致、能编译、能跑"。

---

## 0. C/C++ 边界总览（先建立全局认知）

下面这张表是整个计划里你与 C++ 的"分工边界"。**凡是标注「你主导 / 不碰 C++」的，全部用 Python 完成；标注「边界·借力」的，你提供逻辑与片段、由 AI 或照抄补全语法，你只做验证。**

| 阶段 | 环节 | C/C++ 是否涉及 | 你的边界（不需要深究的部分） |
|---|---|---|---|
| 0 | 硬件链路 / 串流配置 | **否** | 无 |
| 1 | 诊断：确认腿追数据可读 | **是（边界·借力）** | Xr 句柄、扩展枚举、`xrCreateActionSpace` 等当黑盒；你只验证 `isActive`/坐标/速度 |
| 2 | 步态算法原型 | **否（你主导）** | 纯 Python + numpy |
| 3 | 算法翻译成 C++ 类 | **是（边界·借力）** | glm 类型、头文件、引用语法；你只核对数学公式与 Python 一致 |
| 4 | 注入 controls.cpp | **是（边界·借力）** | 插入点行号 + 照抄片段；你核对位置正确 |
| 5 | 设置/标定/UI | **是（边界·借力）** | 序列化机制、imgui 调用；有现成模式可照抄 |

**你不需要理解的 C++ 内部细节（可当黑盒）：**
- OpenXR 的 `XrAction / XrSpace / XrPath / XrSession` 等句柄的生命周期；
- `XR_HTCX_vive_tracker_interaction` 扩展的注册/协商机制；
- `glm::vec2/vec3/quat` 数学库的实现（你只需照原案用同样的公式）；
- CMake / `build_mod.bat` 的构建细节（你只需运行脚本、读报错）。

**你真正要拿稳的能力（你的强项）：**
- 用 Python 把"脚的速度/高度 → 移动向量/按键"这整套逻辑想清楚并验证；
- 把腿追诊断日志/录制数据做成可视化，判断数据是否合理；
- 用电子 DIY 视角理解硬件数据流、排查 SteamVR 绑定、设计物理验证台。

---

## 阶段 0 — 硬件链路与串流配置确认（不写 C++）

**目标**：确认 Pico 腿追数据已经能作为"足部角色"到达 PC 端 OpenXR 之前的所有环节，并锁定运行环境。

**具体任务**
1. 在 SteamVR →「管理 Vive Tracker」中，把两个踝部 tracker 分别指派为 **Left Foot / Right Foot** 角色。
2. 确认 ALVR 的 Headset → Haptics → **Emulation mode = "PICO4"**（你已实测这一步会暴露 Foot 角色）。
3. 确认系统 OpenXR runtime 指向 **SteamVR**（保证 `XR_HTCX_vive_tracker_interaction` 可用）。
4. 记录当前 BetterVR 可正常构建运行（作为"改动前基线"）。

**涉及文件/模块**：无代码改动。仅配置层（`SteamVR` / `ALVR` / 系统 OpenXR 设置）。

**硬件通信接口说明（你的电子 DIY 视角）**
```
Pico 腿追器 ──2.4GHz 私有协议──▶ Pico 头显（端侧 AI 算全身姿态）
        │  ALVR 串流（PICO4 emulation）
        ▼
    PC 端 SteamVR（tracker 作为独立设备，角色 = Left/Right Foot）
        │  OpenXR：`XR_HTCX_vive_tracker_interaction`
        ▼
    BetterVR (C++) 读取 /user/vive_tracker_htcx/role/{left,right}_foot
```
- 你**不需要**写 2.4GHz 配对或 AI 姿态代码——那是 Pico/ALVR 完成的。
- 你**需要**理解这条链路，因为它决定了调试思路：若 `isActive` 读不到，先查 SteamVR 角色指派，再查 ALVR emulation，最后才查 C++ 代码。

**预期产出**：一份《硬件链路检查表》（角色指派、emulation 模式、runtime、基线构建状态），作为后续每阶段的"第一道排查关卡"。

---

## 阶段 1 — 诊断先行：确认腿追数据能被读到（C++ 边界·借力）

> 对应原案 Step 1–2（精简版）。**务必先做这步再写算法**——先确认"数据能读到"，避免下游白做。

**目标**：只做最小 C++ 改动，启用足部扩展 + 创建足部动作 + 每帧定位，把脚部位姿打到日志/调试浮层，**验证 `isActive=true` 且坐标/速度合理**。

**具体任务与文件（均为"照抄原案片段"）**
| 子任务 | 文件 | 动作 | C++ 边界处理 |
|---|---|---|---|
| (a) 探测 Vive Tracker 扩展 | `src/rendering/openxr.cpp`（~33–83 枚举循环） | 仿 `XR_EXT_HP_MIXED_REALITY_CONTROLLER` 写法检测 `XR_HTCX_vive_tracker_interaction` | 照抄原案 (a) 片段 |
| (b) 新增类成员 | `src/rendering/openxr.h` | 加 `m_footPoseAction / m_footPaths / m_footSpaces / m_supportsViveTracker` | 照抄原案 (b) |
| (c) 创建动作 + space + 绑定 | `src/rendering/openxr.cpp` `CreateActions()`；`src/utils/controller_bindings.h` | 仿现有 `createAction`(337) 与 `XrActionSpaceCreateInfo`(440/451)，绑定到 `/user/vive_tracker_htcx/role/{left,right}_foot` | 照抄原案 (c)(d) |
| (d) `Shared` 加足部字段 | `src/rendering/openxr.h`（~65–67） | 加 `footPose / footPoseLocation / footPoseVelocity`（`std::array<…,2>`） | 照抄原案 Step 2 |
| (e) 每帧定位 | `src/rendering/openxr.cpp` `UpdateActions()`（复用 527 行 `locatePose` lambda，在 570/579 调用后） | 对左右脚各调一次 `locatePose` | 照抄原案 Step 2 |
| (f) 调试输出 | 日志 或 `imgui_menus.cpp` 浮层 | 打印 `isActive`、X/Y/Z 坐标、速度幅值 | 简单打印，照抄 |

**你的工作重点（Python 发挥区）**
- 编写 `tools/leg_diag_log.py`：把 BetterVR 输出的足部诊断日志解析为 CSV，再用 matplotlib 画出**左右脚 X/Y/Z 随时间曲线 + 水平速度幅值曲线**。
- 你用这张图判断：站立时数值是否稳定（噪声基线）、抬脚/迈步时是否出现合理变化。这就是你的"数据合理性验证"。

**预期产出**
- 一份诊断报告（Python 图表 + 结论）：确认 `isActive=true`、坐标/速度合理。
- **若 `isActive=false`**：按阶段 0 链路倒查（SteamVR 角色 → ALVR emulation → runtime），先解决硬件层，再回头看代码。

---

## 阶段 2 — 步态算法原型（核心，纯 Python，你主导）

> 对应原案第 3 节算法。**这是你贡献最大、也最该自己写的部分**——完全脱离 C++ 与游戏，用合成/录制数据验证逻辑。

**目标**：用 Python 实现 `LegMotionAnalyzer` 等价物，覆盖 walk / run / jump / crouch 四项判定，并用数据测试通过。

**坐标系约定（来自原案）**：位姿在房间坐标系 `m_stageSpace`（Y-up，单位米）。`hmdPos` 取头显世界坐标；玩家朝向用 `hmdRot` 的 yaw。

**具体任务与文件**
- 新建 `tools/leg_motion_lab.py`（建议单文件起步，后续可拆分）：
  - 数据结构：用 numpy 数组模拟每帧 `footLoc[2]`（x/y/z 位置）、`footVel[2]`（x/y/z 速度）、`hmdPos`（y 高度）、`hmdYaw`（朝向角）。
  - `LegMotionAnalyzer` 类（Python 版），方法对应原案：
    - `walkVector`：平均脚水平速度 `vAvg=(vL+vR)/2` → 取反（脚向后蹬 = 身体前进）→ 按 `hmdYaw` 旋转到玩家前/右 → `×walkSensitivity` → `clamp(-1,1)` → 低于 `deadzone` 归零 → 指数滑动平均低通滤波。
    - `isRun`：当 `|vAvg|` 持续超过 `runSpeedThreshold` → `true`（持续态）。
    - `isJump`：**上升沿触发**——任一脚 Y 在短窗口内抬升超 `jumpLiftThreshold` → 置 `true` 一帧 → 进入 `400ms` 冷却防连跳（状态机）。
    - `isCrouch`：头显高度 `hmdPos.y < standHmdY - crouchDepth`（持续态，用 HMD 高度最稳）。
  - `Calibrate()`：输入一段"站立静止"数据，计算 `standHmdY / standFootY / baseFootSpeed` 基线。
- 测试数据：
  - 合成数据生成器：模拟"原地踏步 / 快踏步 / 起跳 / 屈膝下蹲"四种动作的时间序列。
  - 单元测试：断言四种动作分别触发正确的 `walkVector / isRun / isJump / isCrouch`。

**你的工作重点**
- 算法逻辑、阈值初调、数据可视化、单元测试——**全部你做**。
- 调出一组"默认阈值"：`walkSensitivity / runSpeedThreshold(原案默认1.6) / crouchDepth(0.15) / jumpLiftThreshold(0.08) / deadzone(0.15)`。这些数字将直接喂给阶段 3 的 C++ 配置。

**预期产出**
- 经过单元测试的 Python 算法原型 `leg_motion_lab.py` + 测试数据集 + 调好的默认阈值表。
- 一张"四种动作 → 输出"的可视化图，作为后续和 C++ 版本"数值对齐"的基准。

---

## 阶段 3 — 算法落地到 C++（C++ 边界·借力，翻译）

**目标**：把阶段 2 验证过的 Python 逻辑，**逐公式**翻译成 C++ 的 `leg_motion.h` + `leg_motion.cpp`。

**具体任务**
- 新建 `src/hooking/leg_motion.h` + `leg_motion.cpp`，结构照搬原案 Step 3：
  - `struct LegMotionResult { glm::vec2 walkVector; bool isJump; bool isRun; bool isCrouch; }`
  - `class LegMotionAnalyzer { Reset / Update / Calibrate }`，私有成员放滤波状态、标定基准、上一次脚位、跳跃/下蹲边沿与冷却计时。
- **翻译规则（关键）**：每个 Python 公式 → 对应一行 C++（用 `glm::vec2/vec3/quat`）。数学完全不变，只换语法。

**C++ 边界处理**
- 你提供：阶段 2 的 Python 算法 + 阈值 + "这里用 yaw 旋转、那里用低通滤波"的注释。
- AI / 片段负责：`glm` 类型声明、头文件 include、`std::array`、引用/指针语法。
- **你的验证职责（最重要）**：用阶段 2 的同一份测试数据，分别跑 Python 原型与 C++ 模块，对比 `walkVector / isRun / isJump / isCrouch` 输出数值是否一致（误差在浮点容差内）。不一致就说明翻译有偏差，回到公式核对。

**预期产出**
- 可编译的 `leg_motion` 模块 + 一份"Python 与 C++ 数值对齐"验证记录。

---

## 阶段 4 — 注入 controls.cpp（C++ 边界·借力）

**目标**：在 `hook_InjectXRInput()` 中调用分析器，把 `LegMotionResult` 接到四个既有 VPAD 注入点（已逐行核实）。

**注入点与文件（均照抄原案 Step 4）**
| 控制 | 插入位置 | 代码片段 | 说明 |
|---|---|---|---|
| 走动（叠加左摇杆） | `controls.cpp` 死区之后（~1096 后）、`processJoystickInput`(941) 之前 | `leftStickSource.currentState.x += leg.walkVector.x; .y += leg.walkVector.y;` | 后续 941–953 死区/仿真标志逻辑自动转成 `VPAD_STICK_L_EMULATION_*`，无需改 |
| 跳跃 | ~1144（jump_cancel 附近） | `if (leg.isJump) newXRBtnHold |= VPAD_BUTTON_X;` | 边沿触发一帧 |
| 下蹲 | ~1162（crouch_scopeState 附近） | `if (leg.isCrouch) newXRBtnHold |= VPAD_BUTTON_STICK_L;` | 持续按住=蹲 |
| 奔跑 | ~1189（runState 附近） | `if (leg.isRun) newXRBtnHold |= VPAD_BUTTON_B;` | 持续态 |
| 调用分析器 | `hook_InjectXRInput()` 顶部（~1072 读取 `inputs` 之后） | 取 `inputs.shared.footPoseLocation/Velocity`、`renderer->GetMiddlePose()` 的 hmdPos/hmdRot，调 `analyzer.Update(...)` 得 `leg` | 见原案 Step 4 开头 |

**C++ 边界处理**
- 你负责：定位"插入点行号"（用 IDE 搜索 `VPAD_BUTTON_X` / `VPAD_BUTTON_B` / `processJoystickInput` 即可定位）、给出原案片段。
- AI / 照抄负责：把片段填进正确位置、处理 `footValid` 判断（原案 `if (cfg.legTrackingEnabled && footValid)`）。
- 重点核对：走动向量必须加在**死区之后、941 之前**，否则会被死区吞掉（原案特别提醒）。

**预期产出**：重新构建后的 mod，四项控制在游戏内生效（配合阶段 5 开关）。

---

## 阶段 5 — 设置 / 标定 / UI（C++ 边界·借力 + 你的脚本）

**目标**：加配置项、序列化、菜单 UI、标定流程，让功能可开关、可调参、适配不同身高。

**具体任务**
| 子任务 | 文件 | 动作 |
|---|---|---|
| 配置结构 | `src/utils/mod_settings.h` | 新增 `struct LegTrackingSettings { enabled=false; walkSensitivity=1.0; runSpeedThreshold=1.6; crouchDepth=0.15; jumpLiftThreshold=0.08; deadzone=0.15; calibrated=false; }`（**默认全关**） |
| 序列化 | `src/hooking/settings.cpp` | 照抄现有设置读写机制 |
| 菜单 UI | `src/hooking/imgui_menus.cpp` | 加 "Leg Tracking" 分组：开关、各滑块、Calibrate 按钮、实时诊断浮层 |
| 能力位 | `src/rendering/openxr.h` `Capabilities`(18–31) | 加 `supportsViveTracker`，诊断用 |

**你的工作重点（Python 发挥区）**
- 扩展阶段 2 的 `leg_motion_lab.py`，做一个**"阈值标定推荐器"**：输入不同身高 / 佩戴高度（模拟数据），输出建议的 `crouchDepth / jumpLiftThreshold / runSpeedThreshold` 默认值，减少游戏内反复试错。
- 设计一份"标定 SOP"文档：站立不动 → 按 Calibrate → 记录 `standHmdY/standFootY/baseFootSpeed` → 存盘，后续判定以标定值为准。

**C++ 边界处理**：UI/序列化均有现成模式可照抄；你提供"要哪些控件、默认值多少"，AI 补全 imgui/序列化语法。

**预期产出**：带完整设置、标定、调试浮层的发布版 mod。

---

## 收尾 — 验收清单（逐条自测，附 Python 辅助）

直接对照原案第 6 节，并用阶段 1/2 的 Python 工具辅助：

- [ ] 菜单识别 `supportsViveTracker` 并打印日志（阶段 1 日志脚本辅助）
- [ ] 诊断浮层显示左右脚 `isActive=true`、合理坐标与速度（阶段 1 图表）
- [ ] 原地踏步 → 角色朝面朝方向稳定移动；停下 → 停止（阶段 2 原型已验证 walkVector）
- [ ] 加快踏步/大步 → 进入奔跑（阶段 2 原型已验证 isRun）
- [ ] 双脚快速离地 → 触发一次跳跃（有 400ms 冷却，不连跳）
- [ ] 屈膝下蹲 → 角色下蹲；站直 → 站起（阶段 2 原型已验证 isCrouch）
- [ ] 关闭腿部开关后，原手柄控制完全不受影响（默认 `enabled=false`）
- [ ] 标定后不同身高/佩戴高度下判定稳定（阶段 5 标定推荐器辅助）

---

## 你的"Python 工具箱"汇总（贯穿全程的最大杠杆）

| 工具 | 路径建议 | 阶段 | 作用 |
|---|---|---|---|
| 诊断日志可视化 | `tools/leg_diag_log.py` | 1 | 把 foot 日志 → CSV → 曲线，验证数据合理 |
| 步态算法原型 + 单测 | `tools/leg_motion_lab.py` | 2 | 纯 Python 实现/验证四项判定，产出默认阈值 |
| 阈值标定推荐器 | `tools/leg_motion_lab.py`（扩展） | 5 | 模拟不同身高 → 推荐默认阈值 |

**建议推进顺序（降低返工）**：阶段 0 → 阶段 1（只做诊断，确认数据）→ 阶段 2（算法跑通）→ 阶段 3/4/5（C++ 落地，借力）。顺序错位是最大的返工来源。

---

*本计划基于 `BotW-BetterVR_LegTracking_Plan.md`（源码逐行分析，2026-08 核查）为「无 C/C++ 基础、有 Python/电子 DIY 经验」的开发者改写。所有行号以当时仓库为准，落地时请用 IDE 搜索关键符号（如 `VPAD_BUTTON_X`、`locatePose`、`createAction`）定位。*
