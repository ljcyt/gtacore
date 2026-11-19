# GTA-like 核心状态机（UE5 C++）

M1 垂直切片的两块核心逻辑，可直接编译进 UE5 工程：

| 文件 | 作用 |
|---|---|
| `WantedComponent.h/.cpp` | 通缉系统：热度池 → 星级（1–5），Clean / Suspected / Pursuit / Evading / Busted 五态流转，脱离视线消星、贴身抓捕 |
| `DriveInteractionComponent.h/.cpp` | 步行/驾驶双态切换：找车、上车（附身车辆 Pawn）、下车（三侧胶囊扫落点） |
| `VehiclePawn.h/.cpp` | 可驾驶车辆基类：驾驶座附着点 + 占用标记（车辆物理 Chaos Vehicles 在子类接） |

## 接入方法

1. 把 6 个文件放进工程 `Source/<你的模块>/`，把代码里的 `GTACORE_API` 换成你模块的导出宏（或直接删掉）。
2. 玩家角色蓝图上加 `WantedComponent` + `DriveInteractionComponent` 两个组件；互动键绑 `TryToggleDrive()`。
3. 车辆蓝图父类选 `VehiclePawn`，把 `DriverSeat` 摆到驾驶座位置；车辆碰撞通道需能被 `ECC_Pawn` 扫到。
4. 警察 AI 的契约（Behavior Tree / StateTree 里调）：
   - 感知组件看见玩家 → 每帧 `WantedComponent.NotifySeenByPolice()`
   - 进入抓捕距离 → 每帧 `NotifyBustProgress(DeltaTime, true)`
   - 生成器用 `GetSearchRadius()` 按星级扩围；Evading 时往最后目击点搜索
5. GameMode 监听 `OnWantedChanged`：Busted 时做没收武器/扣钱/医院或警局重生。
6. 犯罪事件接线示例：偷车成功 → `ReportCrime(1)`；开枪命中路人 → `ReportCrime(2)`；击杀 → `ReportCrime(3)`。

## 设计说明

- 星级不用计数器，用**热度池**：小罪攒热度、大罪直接跳级，连环作案自然升星，和 GTA 的手感一致。
- 消星只认"持续脱离视线"，躲进小巷/换车都能生效，因为状态机只看 `TimeSinceSeen`，不关心玩家具体在哪。
- 上下车不做动画状态机也能跑通：附身切换 + 附着座椅就是最小闭环，开门/入座动画是 M2 的表现层，不挡逻辑。
