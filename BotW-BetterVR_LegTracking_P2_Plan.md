# BotW-BetterVR 腿部追踪 · P2 实施方案（步态分析 → 移动注入）

> 状态：已确认（2026-09-06 用户拍板），P2-A 实现完成待采集。
> **范围决策**：只做 **走 / 跑 / 跳**，下蹲功能取消。录制开关放 settings 菜单。走动方向以头显朝向为基准。
> 前置：P0（硬件链路）、P1（XR_HTCX + OpenVR 双后端读取）已实机验证通过。
> 本文档对应 `BotW-BetterVR_LegTracking_DevPlan.md` 阶段 2-5 的落地细化，含两轮实机测试的**用户动作 SOP**。

---

## 1. 现状基线与待解问题

P1 验证结论（2026-09-06 日志）：OpenVR 后端端到端打通，双脚识别（idx=11/13，`ALVR/tracker/left_foot`/`right_foot`），姿态进游戏后持续输出。

**两个未确认项，由 P2-A 采集数据一次性回答：**

| # | 问题 | 影响 |
|---|---|---|
| Q1 | 脚 Y≈0.88 m 偏高（踝部应 ~0.1 m，怀疑 tracker 绑在小腿/膝位） | 算法全部采用**标定相对值**（`standFootY` 基线），绝对高度不影响判定，但抬脚阈值 `jumpLiftThreshold` 的量纲要与佩戴位置匹配 |
| Q2 | `linearVelocity` 走动时是否非零（站立时恒 0 正常，未测动态） | walkVector 核心依赖脚速度；若 OpenVR 速度通道无效，退化为「帧间位移差分」（代码里两手准备，不影响方案） |

## 2. 阶段划分总览

```
P2-A 数据采集模式      C++ 加录制开关 → CSV     【1 轮实机：采集 SOP（第 5 节）】
P2-B Python 算法原型   tools/leg_motion_lab.py   用户主导，AI 辅助；用 A 轮数据调阈值
P2-C C++ 翻译          src/hooking/leg_motion.*  逐公式翻译 + 数值对齐
P2-D 注入 controls.cpp 四个注入点                默认关，开关控制
P2-E 设置/UI/标定       imgui 菜单 + Calibrate    【1 轮实机：验收 SOP（第 6 节）】
```

依赖链：A → B → C → D → E。B/C 可并行准备骨架，但阈值必须等 A 的数据。

---

## 3. 各阶段设计

### P2-A 数据采集模式（C++，AI 填空）

**目标**：按帧把腿部+头显数据写成 CSV，供 Python 离线分析与阈值标定。

**改动**：

| 文件 | 改动 |
|---|---|
| `src/utils/mod_settings.h` | `BoolSetting legTrackingRecordData{ "LegTrackingRecordData", false }`（默认关，不序列化到稳定设置也行） |
| `src/hooking/controls.cpp` | `hook_InjectXRInput()`（:1024）内，读取 `inputs` 之后（~:1080）：录制开关开启且 `gameState.in_game` 时，每帧追加一行 CSV |
| `src/hooking/imgui_menus.cpp` | "Leg Tracking" 分组（:1113）加一个 Record Data 复选框 + 当前文件名显示 |

**CSV 格式**（文件 `Cemu/BetterVR_legrec_<时间戳>.csv`，每次开启新建，避免覆盖）：

```
t,in_game,hmd_x,hmd_y,hmd_z,hmd_yaw,fl_x,fl_y,fl_z,fl_vx,fl_vy,fl_vz,fl_v,fr_x,...,fr_v
```

- `t`：会话相对秒（steady_clock）；`hmd_*`：`renderer->GetMiddlePose()`；`hmd_yaw`：头显朝向角（atan2(fwd.x, fwd.z)）；
- `fl_*`：左脚位置/速度（`inputs.shared.footPoseLocation/footPoseVelocity`），`fl_v`：水平速度幅值，`fr_*` 同理；
- 无效帧（tracker 丢失）写 `nan`，不跳行——保持时间轴完整。

**交付物**：可编译的录制功能 + 一份采集 CSV（用户跑第 5 节 SOP）。

**验收**：CSV 行率 ≈ 游戏帧率；静立段 hSpeed≈0；踏步段 `fl_v/fr_v` 出现交替峰（同时回答 Q1/Q2）。

### P2-B Python 算法原型（用户主导）

**目标**：`tools/leg_motion_lab.py`（新建），实现三项判定（走/跑/跳）+ 用 P2-A 数据调出默认阈值。

算法骨架（沿用原案，标注修正；**下蹲已从范围移除**）：

| 判定 | 规则 | 备注 |
|---|---|---|
| `walkVector` | 位置差分得到双脚速度 → 转到头显局部坐标 → 选当前速度较大的脚，取其向后蹬地分量 → deadzone → 指数滑动平均滤波 | **已按实测修正**：OpenVR 原生速度全为 0；不能平均双脚（交替步态会抵消），先用较强脚的后向冲量生成前进量；后续再用步态周期闸门抑制挥手误报 |
| `isRun` | `|vAvg|` 持续 > `runSpeedThreshold` | 持续态 |
| `isJump` | 任一脚 Y 短窗口抬升 > `jumpLiftThreshold` → 边沿触发一帧 + 400 ms 冷却 | 窗口与阈值以 A 轮数据定 |

`Calibrate()`：静立段自动求 `standHmd_y / standFoot_y / baseFootSpeed`。

**初始阈值**（原案默认，待数据修正）：`walkSensitivity=1.0`、`runSpeedThreshold=1.6`、`jumpLiftThreshold=0.08`、`deadzone=0.15`。

**交付物**：`leg_motion_lab.py`（含 CSV 解析、分析器、分段可视化、单测）+ 阈值表 + 「动作→输出」对照图。

### P2-C C++ 翻译（AI 填空）

`src/hooking/leg_motion.h/.cpp`：`struct LegMotionResult { glm::vec2 walkVector; bool isJump, isRun; }` + `class LegMotionAnalyzer { Reset/Update/Calibrate }`。每条公式对应 Python 一行，数学不变。

**数值对齐验证**：同一份 CSV 喂两个版本（C++ 侧加一个临时 main 或由我写对照驱动），`walkVector` 误差 < 1e-4，布尔量完全一致。不一致=翻译偏差，回公式核对。

### P2-D 注入 controls.cpp（AI 填空）

注入点已逐行核实（2026-09-06）：

| 控制 | 位置 | 注入方式 |
|---|---|---|
| 分析器调用 | `hook_InjectXRInput()` :1080 附近（读 `inputs` 后） | `footValid && legTrackingEnabled` 时 `analyzer.Update(...)` |
| 走动 | **:1116 之后、:1122 之前**（死区+控制器方向旋转块之后，`processJoystickInput` :1231 之前） | `leftStickSource.currentState.x/y += leg.walkVector.x/y`。放在旋转块**之后**，避免 CONTROLLER 方向模式下双重旋转；走动方向以头显朝向为基准（已确认） |
| 跳跃 | :1144（`jump_cancel` → `VPAD_BUTTON_X`） | `if (leg.isJump) newXRBtnHold \|= VPAD_BUTTON_X;`（一帧） |
| 奔跑 | :1186-1190（`VPAD_BUTTON_B`） | `if (leg.isRun) newXRBtnHold \|= VPAD_BUTTON_B;`（持续） |

**防误触发**：`!gameState.in_game` 时不调用分析器（菜单内踏步不动作）；`isJump` 冷却 400 ms 防连跳。

**验收**：编译通过；三项控制生效；关闭 `legTrackingEnabled` 后手柄行为与基线完全一致。

### P2-E 设置 / 标定 / UI（AI 填空）

| 子任务 | 文件 | 内容 |
|---|---|---|
| 配置 | `mod_settings.h` | `LegTrackingSettings`：`enabled=false`（默认关）、各阈值、`calibrated` 标志 |
| UI | `imgui_menus.cpp` Leg Tracking 分组 | 开关、阈值滑块、Calibrate 按钮（采集静立 3 s 求基线）、录制开关（P2-A 已有）、实时诊断浮层（walkVector/isRun/isJump + 脚速度条） |
| 标定 | `leg_motion.cpp` | Calibrate 用滑动窗口静立检测，结果存设置 |

---

## 4. 已确认的决策（2026-09-06）

1. **范围**：走 / 跑 / 跳。下蹲功能取消（BotW 下蹲是 toggle 交互，边沿注入方案随之作废）。
2. **录制开关**：settings 菜单 → Controls → Leg Tracking → Record Leg Data（session 级，每次启动重置为关）。
3. **走动方向**：以头显朝向为基准（转头 = 改变移动方向）。

---

## 5. 实机测试 SOP · 第一轮（P2-A 数据采集，约 10 分钟）

**准备（做动作前）**
- [ ] 两个 tracker 绑定位置**拍照或记录**（踝上/小腿中段/膝下——直接决定 Q1 结论与 `jumpLiftThreshold` 量纲）
- [ ] SteamVR → 管理 Vive Tracker：Left Foot / Right Foot 角色确认
- [ ] 启动游戏，读取存档，传送到**开阔平地**（推荐初始台地或平原塔附近）
- [ ] 打开 BetterVR 菜单 → Controls → Leg Tracking → 勾选 **Record Data**
- [ ] 站到房间中心，确认前方 1 m 内无障碍物

**动作序列**（每个动作之间必须**静止站立 5 秒**作为分隔符，我靠它自动分段）：

| # | 动作 | 时长 | 验证目标 |
|---|---|---|---|
| 1 | 静止站立（自然姿势，不要僵直） | 30 s | 噪声基线、standHmdY/standFootY |
| 2 | 原地慢走（自然步频） | 30 s | walkVector 低速段 |
| 3 | 原地快走 | 30 s | walk→run 过渡区 |
| 4 | 原地跑步（高步频、脚抬高） | 30 s | isRun 阈值上限 |
| 5 | 原地单次跳（双脚同时离地）×5，间隔 3 s | ~20 s | isJump 上升沿 + Y 抬升量 |
| 6 | 连续小跳 | 10 s | 冷却逻辑必要性 |
| 7 | 面向北原地踏步 15 s → 原地转身 180° 面向南再踏步 15 s | 30 s | hmd_yaw 旋转正确性 |
| 8 | 站立不动，**只挥手/扭腰/转身头** | 15 s | 上半身动作不误触发 |
| 9 | 静止站立收尾 | 10 s | 漂移检查 |

**结束后**：菜单里取消 Record Leg Data → 退出游戏 → 把 `Cemu/BetterVR_legrec_*.csv` 发我（顺带 `BetterVR_log.txt`）。

**注意**：动作 7 不需要走动，全程原地；房间不够大就保持原地，转身即可。

---

## 6. 实机测试 SOP · 第二轮（P2-E 验收，约 8 分钟）

前置：P2-D/E 完成、阈值已按第一轮数据标定、`legTrackingEnabled=on`、Calibrate 已执行。

| # | 你的动作 | 预期角色行为 |
|---|---|---|
| 1 | 静立 10 s | 完全不动（无漂移） |
| 2 | 原地自然踏步 | 朝面朝方向匀速走；停下 → 立即停 |
| 3 | 加快步频 | 过渡到跑 |
| 4 | 停止踏步后立即单跳 | 跳一次；落地 400 ms 内再跳无效 |
| 5 | 边踏步边轻推左摇杆 | 向量叠加（方向/速度合成合理） |
| 6 | 打开物品菜单踏步 | 菜单内角色不动（防误触发） |
| 7 | 菜单关闭 legTrackingEnabled，踏步 | 无反应；手柄全部功能与原版一致 |
| 8 | 连玩 10 分钟自由游戏 | 无累积漂移、无卡顿（性能回归） |

**通过标准**：8 项全过。#2 的方向偏差 > 15° 或 #8 出现明显掉帧（>5 fps）为不通过，回报日志。

---

## 7. 风险与开放问题

| 风险 | 缓解 |
|---|---|
| OpenVR 速度通道无效（Q2） | P2-B 备好位置差分方案，同一数据流切换 |
| 踝部 tracker 抬脚 Y 变化量太小（<0.05 m） | 用「速度突增 + 双脚同时离地」复合判据，第一轮数据定 |
| 原地踏步的脚部水平速度均值≈0（前后摆动抵消） | 取「负向速度段」（蹬地相）均值而非全窗均值——P2-B 里两种算法都跑，数据说话 |
| BotW 帧率与输入帧不同步导致 walkVector 抖动 | 指数滑动平均（α 待定，原型调） |
| tracker 佩戴在膝盖时的跳跃误判 | 跳跃判据以「脚速度突增 + 双脚同时离地」为主、Y 抬升为辅，降低对绝对高度的依赖 |

---

## 8. 里程碑

| 里程碑 | 内容 | 门槛 |
|---|---|---|
| M1 | P2-A 完成编译 + 第一轮采集 SOP 跑完 | CSV 数据有效（帧率、分隔符清晰） |
| M2 | P2-B 阈值表定稿 | 三项判定（走/跑/跳）在采集数据上 100% 分段正确 |
| M3 | P2-C 数值对齐通过 | walkVector 误差 < 1e-4 |
| M4 | P2-D 注入完成、默认关 | 编译通过、开关关闭=基线行为 |
| M5 | P2-E 验收 SOP 9/9 通过 | 第二轮实机全过 |

M1 之后我就可以并行准备 P2-C 的 C++ 骨架（阈值留空），等你 B 轮数据填数。
