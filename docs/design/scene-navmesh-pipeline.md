# 场景导航网格管线(Recast 烘焙 + 服务器寻路阻挡校验)

日期:2026-08-26。状态:代码就位,**未编译,待 Codex 验证**(见 §6)。

## 1. 背景与问题

- 服务器启动时按 `BaseScene` 表 `nav_bin_file` 加载 `data/scene_nav_bin/*.bin`
  (`spatial/system/navigation.cpp`),但 `navQuery` 全仓只有 init,从未被查询——
  导航是死数据。
- 旧的三个 bin(main/dungeon/mirror)**md5 完全相同**,是 UE 客户端时代的占位
  数据(UE 布局、厘米单位、agent 144/35/35),与现在 Unity 客户端的天墉城
  400×300 地图毫无关系,且仓库内没有任何能重新生成它们的工具。
- 移动 handler(`player_movement_handler.cpp`)此前全部空桩,服务器收到移动包
  即丢;`MovementSystem` 无条件积分,没有任何阻挡校验。
- 客户端权威阻挡数据 = `TianyongPaintedCity.cs` 内嵌的 `WalkMaskBase64`
  (150×150 位图,单格 2m,painted-city 主题默认启用);两端此前毫无关联。

## 2. 管线全貌

```
mmorpg-client/TianyongPaintedCity.cs (WalkMaskBase64, 权威阻挡真相)
        │ tools/navmesh_baker --painted-city  (直接解析 C# 源里的 base64)
        ▼
可走格 → 地面三角面(不可走=留洞) → Recast tile 烘焙(ue5navmesh 同源码)
        │ --out
        ▼
data/scene_nav_bin/tianyong_scene.bin ('MSET' v1,与 recast.cpp 加载器逐字节兼容)
        │ 复制为 main/dungeon/mirror 三个文件名(当前所有场景同一张图)
        ▼
scene 节点 LoadNavBins → SceneNavManager → NavQuerySystem(吸附/阻挡射线/寻路)
        ├─ player_movement_handler:MoveStart/Sync/Stop 位置裁决 + MoveAck 纠偏
        └─ MovementSystem:每 tick 积分后的阻挡夹持(撞墙停在阻挡点并清速度)
```

未来真 3D 场景:Unity 导出 OBJ,走 `navmesh_baker --obj` 同一条烘焙路径。

## 3. 关键决策

| 决策 | 选择 | 原因 |
|---|---|---|
| Recast 源码 | 沿用 `third_party/ue5navmesh`(dtReal=double,WITH_NAVMESH_* 全不定义) | 运行时已用它编译;烘焙器与运行时**必须同一套布局**,否则 bin 二进制不兼容。原版 recastnavigation 的 float 版 `dtNavMeshParams` 与之不通 |
| 烘焙输入 | 直接解析客户端 C# 源里的 WalkMaskBase64,不开 Unity | 地图是运行时程序化生成,编辑期场景无几何;mask 就是客户端判定真相,解析它 = 两端语义一比一,且无 Unity/Python 依赖 |
| agent radius | 0(不收缩) | 客户端 `TianyongPlayerController` 只拿脚底点查 mask,无半径概念;服务器校验同一个点。收缩会造成"服务器说撞墙、客户端看还有缝" |
| 单位/坐标 | 米,Unity 坐标系(Y-up)烘焙;查询侧换轴 | 换轴契约与客户端 `WorldCoordinateConverter` 一致:nav=(sy,sz,sx),server=(nav.z,nav.x,nav.y)。服务器是 Z-up |
| cs | 0.25m | mask 格 2m 的整数分之一,导航边界与 mask 格线对齐;**注意** `rcFilterLedgeSpans` 会把紧邻洞的一圈体素当悬崖剔掉,实际边界向内缩 1 个体素(0.25m),`kMoveCorrectionEpsilon` 取 0.5m 吞掉这个差值(2026-09-05 审查确认,见 nav-spawn-fix 文档 §8) |
| 校验失败策略 | fail-open(场景无导航→放行)+ 阻挡点截断 + MoveAck 纠偏 | 不能因缺数据卡死玩家;撞墙时服务器权威位置停在阻挡点,水平差 >0.25m 回 MoveAck 让客户端拉回 |

Agent 参数:height 1.8 / climb 0.35(对齐客户端 `TianyongMapConfig`
playerHeight/playerStepOffset)。速度信任上限 `kMaxTrustedClientSpeed=10`
(客户端 moveSpeed 9 + 余量,见 `spatial/constants/nav.h`)。

## 4. 本轮代码改动

新增:
- `tools/navmesh_baker/navmesh_baker.cpp` + `CMakeLists.txt`:离线烘焙器
  (mask/OBJ → tile 烘焙 → MSET v1)。
- `cpp/libs/services/scene/spatial/system/nav_query.{h,cpp}`:`NavQuerySystem`
  ——SnapToMesh / ValidateMove(返回阻挡点)/ FindPath(拉直路径),
  对外收发服务器坐标,内部换轴。

修改:
- `spatial/manager/scene_nav.h`:补 `Get(id)`。
- `spatial/constants/nav.h`:速度上限 / 纠偏阈值常量。
- `spatial/system/movement.cpp`:积分后导航夹持 + Transform/Velocity 脏位
  (补上了此前注释里欠的 `kTransformFieldNumber` 脏位)。
- `cpp/nodes/scene/handler/rpc/player/player_movement_handler.cpp`:
  MoveStart/MoveSync/MoveStop 落地(位置裁决 + 速度截断 + MoveAck)。
  TeleportRequest 仍为空桩(需传送表,独立特性)。
- `scene` 库 CMakeLists / vcxproj / filters:注册 nav_query。

## 5. 已知边界(不在本轮)

- ~~首进场 spawn 契约仍是 (0,0,0)~~ **2026-09-05 已修**(见
  `nav-spawn-fix-2026-09-05.md`):`SceneSpawnSystem::EnsureValidEnterLocation`
  在 `HandleEnterScene` 里按导航校验进场位置,不合法落到出生点
  `kTianyongSpawn* = (180,200,0)`(== Unity (200,0,180));`LoadNavBins` 注册前
  探针出生点,旧占位 bin 会被拒绝注册;烘焙器 `--probe` 出包前同样探针。
  出生点仍是常量而非 BaseScene 表列(所有场景共用一张图)。
- 客户端未做预测回滚,MoveAck 纠偏 >1.5m 硬 snap(GameClient.cs 现状);
  snap 落点不在 mask 上时客户端会恢复到最近可走格并回报 MoveStop(兜底,
  正常数据下不触发)。
- AI 寻路(`FindPath`)已有 API,尚无调用方;dtCrowd 仍未接。
- 反作弊只有速度截断 + 阻挡夹持,无 client_time 单调性/违规计数。
- `SceneNavManager` 是 thread_local,`LoadNavBins` 只在启动线程跑过——多场景
  线程模型下需要每线程加载(现状如此,本轮未改)。

## 6. Codex 执行清单(编译 + 出数据 + 验证)

顺序执行,C++ MSBuild 串行 `/m:1`:

1. **烘焙器构建**(工作目录 `xuanming-server-mmo`):
   ```
   cmake -S tools/navmesh_baker -B tools/navmesh_baker/build
   cmake --build tools/navmesh_baker/build --config Release
   ```
   期望产物 `tools/navmesh_baker/build/Release/navmesh_baker.exe`,零 error。
2. **烘焙**(工作目录 `xuanming-server-mmo`,客户端仓在同级 `../mmorpg-client`):
   ```
   tools\navmesh_baker\build\Release\navmesh_baker.exe --painted-city ..\mmorpg-client\Assets\Scripts\World\Tianyong\TianyongPaintedCity.cs --out data\scene_nav_bin\tianyong_scene.bin
   copy /Y data\scene_nav_bin\tianyong_scene.bin data\scene_nav_bin\main_scene.bin
   copy /Y data\scene_nav_bin\tianyong_scene.bin data\scene_nav_bin\dungeon_scene.bin
   copy /Y data\scene_nav_bin\tianyong_scene.bin data\scene_nav_bin\mirror_scene.bin
   ```
   通过标准:stdout 有 `mask geometry: ... quads`、`built N tiles`(N>0)、
   `saved ...: N tiles`;bin 尺寸不再是旧的 20648 字节。
3. **scene 编译**:重编 `scene` 静态库与 `scene` 节点(msbuild game.sln 对应
   工程,`/m:1`)。新文件 nav_query.cpp 已注册进 CMake/vcxproj/filters。
4. **冒烟**:起本地栈(dev.bat 路径),客户端或 robot 进场后:
   - scene 日志无 `nav bin load failed` / `skip nav registration`;
   - 客户端朝城墙/建筑走,观察收到 MoveAck 且位置被拉回(或 robot 侧断言
     MoveSync 上报非法点后收到 MoveAckS2C.server_location ≠ 上报点);
   - 沿街道正常行走不应收到纠偏 MoveAck。
   失败时保留 scene 节点 stdout/log 尾部 200 行。


## 7. 2026-09-12 三地节庆地图接入

三地的日景和节庆画面各两张,同一地区共享一份保守可走位图。地图资源与
原图尺寸不决定服务器的单位:仍沿用 Unity 米制 300×300 地面、x=50–350、
z=0–300,150×150 位图每格 2m、从画面上方向下排列、MSB-first 共 2813 字节。

| 场景配置 ID | 地点 | nav_bin_file 文件名 | Unity 出生点 | 服务器出生点 |
|---|---|---|---|---|
| 1 及原有其它场景 | 天墉城 | main/dungeon/mirror_scene.bin | (200,0,180) | (180,200,0) |
| 2 | 蓬莱岛 | penglai_scene.bin | (220,0,170) | (170,220,0) |
| 3 | 东海渔村 | donghai_scene.bin | (200,0,180) | (180,200,0) |
| 4 | 揽仙镇 | lanxian_scene.bin | (200,0,180) | (180,200,0) |

这些 ID 已在 World 表中,继续走现有 EnterSceneC2S(scene_config_id, scene_id=0)
路由,无需新增协议或临时预览场景。日景/节庆切换是客户端画面状态,不新建网络场景。
同节点传送到不同配置地图时采用目标出生点;同配置换线、重登仍保留合法保存位置。

出生坐标从 `data/schema/basescene_table.proto` 与 `data/BaseScene.xlsx` 正式增加
`spawn_x/y/z` 三个 double 字段,字段号 3/4/5。唯一消费点仍为
`SceneSpawnSystem::DefaultSpawnFor`。无场景/旧表无出生列时保留天墉兼容值;
所有正式行都显式填值。网格文件按路径只加载一次,但每行出生点都要通过探针才注册。

新地图位图在独立客户端的
`Assets/Resources/World/FestivalRegions/{penglai,donghai,lanxian}/walkmask.txt`。
烘焙器 `--mask-base64 <文件>` 直接接受这些资源,与旧 C# 入口走同一几何管线。
此模式必须显式 `--probe x,y,z`,不偷偷套天墉出生点,且拒绝错误长度/编码、空几何、
不在网格的探针以及非有限坐标。`--painted-city` 的原用法与默认天墉探针保持兼容。

示例(工作目录为服务端仓库):

```powershell
tools/navmesh_baker/build/Release/navmesh_baker.exe --mask-base64 ../mmorpg-client/Assets/Resources/World/FestivalRegions/penglai/walkmask.txt --probe 220,0,170 --out data/scene_nav_bin/penglai_scene.bin
```

出包后须先重编并部署所有读取 BaseScene 的服务,再重启本地 scene 加载配置与网格。
Go 的 JSON 表解析拒绝未知字段,因此本机 scene_manager/player_locator/login 也须更新;
friend/guild 如有部署同样需要重编。仅复制新 JSON 给旧 Go 程序会在启动时拒绝 spawn_x。
不要清理数据库或重置角色;
旧地图非法存档坐标由进场逻辑自然修复。验证入口为烘焙器的 `test_baker.py`、
现有 bag_test 工程中的独立 `SceneSpawnTest` 和客户端真实登录/传送/移动验收。


## 8. 2026-09-13 接入验证

- 三份最终位图与客户端资源逐字节一致,蓬莱/东海/揽仙分别含 2265/2218/2295 个可走格,
  烘焙为 34/40/45 个 tile,文件为 86064/80520/102088 字节。保留不收缩边缘的旧烘焙规则;
  各自出生探针返回精确配置坐标,天墉及原有其它场景继续使用 61 tile 网格。
- `table`、`scene` 静态库、`scene` 节点及 `bag_test` 四个工程按 Debug/x64、`/m:1`
  串行构建通过。完整 `bag_test` 为 220/220,其中 `SceneSpawnTest` 6/6 覆盖 21 行导航
  注册、三图网格独立、非法存档修复、同图重入保位、跨图出生及无导航兼容。
  烘焙器契约测试 6/6,覆盖独立位图与旧入口一致、缺失/非法出生、坏编码及空几何拒绝。
- 导表 manifest v13 的 28 张表所有源文件、JSON/PB 产物大小与 SHA256 匹配;
  正式新增 BaseScene 的 Go 绑定及五个读取者 scene_manager/player_locator/login/friend/guild
  均编译通过;前三个已部署本机运行目录,后两个未启用。Go 表包既有测试仍引用已经移除的
  TestMultiKey/Buff `FindBy*` API 而无法编译,本轮未更改这些测试或对应表,不计入通过项。
- 证据位于工作区 `../tmp/festival-final-*`,含构建日志、C++ XML、烘焙日志及
  `festival-final-server-validation.json`(位图与 bin 哈希对应)。独立代码复核未发现本轮
  同节点传送阻塞问题;跨节点切换仍遵守既有的上游安全门禁。

- 正式客户端首轮登录成功后,传送到配置2停在加载态:Go 已发送新 RoutePlayer,但 gate
  只在待登录类型非零时通知 scene。修复后同节点不同场景实例使用 LOGIN_NONE 入场,
  不重复触发登录事件;相同路由重投不重复入场。发送前置检查失败不提交新场景或消费待
  登录类型,缺少场景实例的 BindSession 继续等待。gate 串行重编通过,路由身份测试26/26,
  其中7条覆盖两种登录顺序、重复投递、跨图、同图换线和暂缺RPC连接后的重投恢复。

- 正式客户端最终联机验收 PASS:2→3→4→1 四次服务端入场、三地日景/节庆与返城7张截图;各图出生误差、导航不一致和移动重定位均为0。服务端入场原始行、进程PID/启动时间、4图网格及BaseScene运行文件SHA256收在 `../tmp/festival-final-server-deployment-evidence.json/.log`。

## 9. 2026-10-18 当前三张地图替换正式场景

正式场景 1/2/3 改用客户端 `World/CurrentMaps20261009` 的主城、八仙岛、渔村。
这些是当前已制作布局的先行接入,美术合同标为 `layout_preview`;
300×300 米是本次接入尺度,不是容量设计验收结论。新导航只由各图的
`walkmask.txt` 烘焙,不复用前述天墉/蓬莱/东海旧画面位图。

| 场景 ID | 客户端 key | 正式导航文件 | Unity 出生 | 服务端出生 |
|---|---|---|---|---|
| 1 | main | main_scene.bin | (199,0,133) | (133,199,0) |
| 2 | island | penglai_scene.bin | (195,0,173) | (173,195,0) |
| 3 | village | donghai_scene.bin | (201,0,147) | (147,201,0) |

导航文件名保留既有 BaseScene 路径契约;`penglai_scene.bin` 此后装的是八仙岛新图。
源表 5–16 也引用 `main_scene.bin`,因此出生点一并采用新主城坐标,避免加载同一网格时
其它表行的出生探针失败。场景 4、17–21 及其旧导航保持原样;当前客户端不开放无新资源地图。
登录请求不携带存档场景 ID:login 的 `resolveEnterSceneRoute` 返回 sceneID=0,
scene_manager 的 `resolveSceneForEnter(0,0,zone)` 从 World 第一行选择正式场景 1,
因此旧账号正常登录由服务器进入新主城。跨区传送中已有的 pending 目标仍遵守既有票据规则。

`tools/navmesh_baker/sync_current_maps.py` 维护可重复管线:

1. 从同级客户端各图 `source.json` 核对场景 ID、世界矩形、位图与图片 SHA256、
   22500 位长度/尾部零填充、可走格数及出生点;任何不一致先拒绝,不写正式文件。
2. 从三份新位图烘焙,逐一验证出生点不需要吸附,并对最终 Detour 网格查询全部
   22500 个格心。三图全部成功后才替换正式 bin 并修改 `data/BaseScene.xlsx`。
3. 同步 `data/scene_nav_bin/current_maps/{main,island,village}/{source.json,walkmask.txt}`
   作为跨语言合同产物,与客户端逐字节一致;Java ContractSync 复制到对应
   `config-data/navmesh/current_maps/`。这些是派生产物,不可在服务端单独改图或改出生点。
4. 按既有正式导表器生成 BaseScene JSON/PB 与 manifest;没有修改 schema 或手改生成表。
   `--verify` 重新烘焙并比较 bin 全字节、合同副本、源表、生成 JSON 和清单中 JSON/PB 的 SHA。

```powershell
cmake -S tools/navmesh_baker -B tools/navmesh_baker/build
cmake --build tools/navmesh_baker/build --config Release -- /m:1
python tools/navmesh_baker/sync_current_maps.py
# 按本机已有工具链运行正式导表器,不是手工编辑 generated/tables。
python tools/data_table_exporter/run.py
python tools/navmesh_baker/sync_current_maps.py --verify
python -m unittest discover -s tools/navmesh_baker -p "test_*.py"
```

本次实际使用正规导表器的沙盒配置,只生成数据与用于二进制导表的 Python proto;
源 schema 未变,其它语言代码无需重生成。核对只有 `basescene.json`、`basescene.pb`
和 `manifest.json` 改变后采用这三个产物。最终先快进到服务端主干 `e4aee007f`,
再从最新 45 张源表重新导出;44 张非 BaseScene 表和它们的清单条目与主干完全一致,
manifest 从 28 升为 29,保留排行榜、邮件、装备增强等主干配置。
三图分别 55/40/47 tile、6651/4018/4524 个可走格;67500 格心差异为 0。
MSVC Release 烘焙器编译、13 项合同测试、重烘焙字节一致和 schema 索引检查通过。
证据清单在 `data/scene_nav_bin/current_maps_20261009.json`,本机导表日志在
`../tmp/current-map-server-20261018/export-latest.log`,采用对账在
`export-latest-adoption.json`。schema 索引首次检查发现主干的 MessageLimiter 条数仍写 81,
实际已有 87 行;按正式索引生成器更新该文档行后复查通过,未修改 MessageLimiter 源表。

本次没有运行完整 scene/gate 服务或 Unity 联机验收:本机固件虚拟化关闭、Docker
daemon 未运行,且 `bin/gate.exe`、`bin/scene.exe`、`bin/battle.exe` 缺失。
上述 9/13 的联机成功记录属于旧地图,不能作为这次新地图的联机验收证据。
