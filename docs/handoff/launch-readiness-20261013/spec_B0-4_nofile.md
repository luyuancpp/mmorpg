# 调研规格:gate / scene / battle 的 RLIMIT_NOFILE 与启动断言(2026-10-16)

> **状态**:只读调研产出,**未落码**。由开服线会话 2026-10-16 的调研工作流生成(一名调研者出规格、一名核实者逐条反驳),原样保存,未经人工改写。
> **怎么用**:前半部分是调研者的规格;「# VERDICT」之后是核实者意见。**落码前必须先吸收核实意见里的 must / should**,被判 refuted 的事实以核实者为准。行号与工作区状态是 2026-10-16 03:48~04:35 的快照,工作区有多个会话在并行修改,落码前按函数名重新定位。
> **汇总、拍板项与推荐顺序**:见 [docs/PROGRESS.md](../../PROGRESS.md)「2026-10-16 开服线剩余工作调研」条目,以及交接 [handoff-launch-readiness-and-d1-20261013.md](../handoff-launch-readiness-and-d1-20261013.md) 文首 10-16 段。

## item
gate / scene / battle 容器补 RLIMIT_NOFILE，并加启动断言（容量模型 §1.3 第 2 条、A0-2；gate 准入设计 G4 / P0-1；全局单点交接 §3 P1「gate 未设 nofile」、§7 阶段 1。10-16 起归开服就绪线）
## already_done
无。我在工作区和 HEAD 里都核过：cpp/ 下除生成目录和第三方以外，setrlimit 只出现在 Windows 适配层（cpp/libs/engine/muduo_windows/src/muduo/base/CrossPlatformAdapterFunction.cc:234），没有任何 getrlimit/setrlimit 调用。docs/PROGRESS.md 10-13 之后没有 nofile/RLIMIT 的落码条目，只有 :7343、:7370 两处登记。docs/handoff/launch-readiness-20261013/ 下没有这一项的规格。相邻的、已经做掉的是另外两件事，和本项无关、也不要合并：Redis maxclients 加启动打印 fd 限值（P0-E，redis.yaml:99-133，未验证），以及 kafka.yaml:166-185 的 ulimit 启动壳（只管 broker）。
## summary
问题：gate、scene、battle 三类 C++ 进程从来不设 RLIMIT_NOFILE。K8s 没有按容器声明 ulimit 的字段；Dockerfile.runtime 和 Dockerfile.cpp 只写了 USER 10001，没有 ENTRYPOINT，也没有 ulimit；三种启动形态（Deployment、gate StatefulSet、Agones Fleet / hostPort）都是 `/bin/sh -lc "mkdir … && <清理循环> && ./gate"` 一层壳，同样没有 ulimit。所以生效的软限完全取决于节点上的容器运行时。按外部常识（本仓未核实），containerd 2.x 或 Docker 25 以上，配 systemd 240 以上，给的是软限 1024、硬限 524288。这样一台 gate 实际最多撑约 1000 条连接，而配置的是 20000，GateMaxConnections 那条判断永远走不到。fd 用尽后会接连触发 accept 忙循环、日志刷屏、idleFd 失效（G7/G8）。scene 在 2000 万在线时，到 SM/DS 的全服网状连接约 4Z = 800~1780 条（Z = PCU / C_z），同样超过 1024。

根因：fd 上限这件事没有任何一层负责。镜像不管，清单无法声明，进程也不调。

做法：在引擎的 Node::Initialize 里统一处理。三类节点都经过这里，时机放在 LoadConfigs / InitLogSystem 之后、发起任何网络连接之前。新建一个只含纯函数的头文件 open_file_limit.h。node.cpp 在 Linux 上执行：getrlimit，把软限抬到 min(硬限, 1048576)，只抬不降，硬限保持不变（非特权进程也能做）；然后算需求 = 客户端面准入上限 + 固定余量 4096（gate 取 GateMaxConnections，battle 取 BattleMaxConnections，scene 取 0）；在 stdout 和日志各打一行 fd 限值；抬完仍不够就经现有的 FatalStartupConfig 先写 stderr 再 LOG_FATAL，拒绝启动。这一步是 fail-closed，并且和运行模式无关。Windows 下用 #ifdef __linux__ 包住，不做任何事。不改 K8s 模板、proto 和配置键。硬限只能在节点运行时层面调，这一点写进运维文档，不在仓库清单里处理。
## est_files: 16
## current_facts
F1. 三类 C++ 节点都在 Node 的类型构造函数里先写入 node_type，再调 Initialize；Initialize 的顺序是 LoadConfigs → InitLogSystem → grpc_call_deadline::Apply → InitRpcServer（连 Redis）→ LoadAllConfigData → InitKafka → InitEtcdService。所以 :513 和 :516 之间是第一个既读到了配置、又还没有任何网络 fd 的位置
   证据: cpp/libs/engine/core/node/system/node/node.cpp:418-430（:428 set_node_type，:430 Initialize）、:497-521（:512 LoadConfigs、:513 InitLogSystem、:516 grpc_call_deadline::Apply、:517 InitRpcServer、:519 InitKafka、:520 InitEtcdService）。亲自打开看过
F2. gate 和 battle 走 RunSimpleNodeMainWithOwnedContext，先构造 Node 再执行 configure；scene 的 main 直接构造 Node。现有的 ValidateGateConnectionLimitOrDie 在 configure 里，比 Initialize 晚，那时 Redis、Kafka、etcd 都已经初始化过了
   证据: cpp/libs/engine/core/node/system/node/node_entry.h:174-193；cpp/nodes/scene/main.cpp:90-106；cpp/nodes/gate/main.cpp:97-138（ValidateGateConnectionLimitOrDie）、:243（调用点）。亲自打开看过
F3. 在 Linux 上，muduo 日志只写异步文件，不进 stdout。LOG_FATAL 写进异步缓冲后，g_flush 只冲 stdout，随即 abort，这条 FATAL 很可能落不了盘。node.cpp 现有的 FatalStartupConfig 是先 fprintf(stderr)+fflush，再 LOG_FATAL，可以直接复用
   证据: node.cpp:1367-1377（AsyncOutput 只在 WIN32 下调 LogToConsole）、:366-372（FatalStartupConfig）；third_party/muduo-linux/muduo/base/Logging.cc:196-205（~Logger 遇 FATAL 时 g_flush()+abort()）。全文件 grep 不到 setFlush。亲自打开看过
F4. 准入上限的配置源：GateMaxConnections 为 20000，BattleMaxConnections 为 4096。K8s ConfigMap 从 bin/etc 原样镜像这两个值；battle 的值在部署时校验必须落在 1..65535
   证据: bin/etc/base_deploy_config.yaml:125-139（:139 GateMaxConnections: 20000；:134-135 注释写着「ulimit -n 要大于本值 + 节点间连接 + 日志等」）、:147-149（:149 BattleMaxConnections: 4096）；cpp/libs/engine/config/config.cpp:105-118；tools/scripts/k8s_deploy.ps1:3154、:3212-3216、:3264。亲自打开看过
F5. 值为 0 时的语义：gate 回落到 session-id 硬上限 131071，battle 回落到 BattleClientEdge::kHardMaxConnections = 65535。只有 dev/test 允许配 0，prod 由各节点自己的启动门禁拒绝
   证据: cpp/nodes/gate/main.cpp:106-125；cpp/nodes/battle/client/battle_client_edge.h:57；battle_client_edge.cpp:164-165；cpp/nodes/battle/main.cpp:177-179。亲自打开看过
F6. K8s 上三类 C++ 节点的所有启动形态都是 `/bin/sh -lc` 外壳加启动命令，都没有 ulimit。gate 和 scene 不 exec，battle 用 exec。因此 gate/scene 的 PID 1 可能是 sh（dash 会不会对最后一条命令做 exec 优化，未核实），验收时看 /proc/1/limits 不可靠
   证据: tools/scripts/k8s_deploy.ps1:3555-3556（New-NodeDeploymentYaml）、:4088-4089（New-SceneFleetYaml）、:7924（./gate）、:7934/:7946（./scene）、:8238-8249（exec ./battle）；tools/scripts/lib/k8s_client_entry.ps1:700-706（Get-ClientEntryStartScript）、:871（gate StatefulSet）、:1097（battle Fleet）、:1209（hostPort）。亲自打开看过
F7. C++ 的静态清单不在 deploy/k8s/manifests/** 下，C++ 工作负载全部由 k8s_deploy.ps1 和 lib 生成。镜像以非 root 运行，没有 ENTRYPOINT，也没有 ulimit，所以进程不能抬硬限，只能把软限抬到硬限
   证据: ls deploy/k8s/manifests/{infra,go-svc,java-svc} 没有 gate/scene/battle 的工作负载；deploy/k8s/Dockerfile.runtime:74、deploy/k8s/Dockerfile.cpp:279（USER 10001:10001）。亲自打开看过
F8. K8s 不注入 GATE_RUN_MODE、SCENE_RUN_MODE、BATTLE_RUN_MODE，K8s 上一律按 prod 运行。scene 的运行模式同时决定 GM 指令放不放行，所以如果用运行模式来豁免 fd 检查，运维就得同时打开 GM，这两件事不能绑在一起
   证据: tools/scripts/k8s_deploy.ps1:3184-3187、:3204；cpp/nodes/scene/handler/rpc/player/player_gm_guard.h:31-44。亲自打开看过
F9. 已有的 ulimit 外壳先例：kafka.yaml 在 exec 前执行 `ulimit -n "$hard"`；redis.yaml 只打印 soft/hard/need，不自己改 nofile，硬限不够时只报 ERROR 不拒启（单实例，有意 fail-open）
   证据: deploy/k8s/manifests/infra/kafka.yaml:166-185；deploy/k8s/manifests/infra/redis.yaml:99-133（:121 MAXCLIENTS=40000）。亲自打开看过
F10. 现有设计结论：G4 的判定是「RLIMIT_NOFILE 全链路无人设」；P0-1 的要求是 getrlimit，软限不到 GateMaxConnections + 余量就拒绝启动，并把软限抬到硬限，不走 initContainer + privileged
   证据: docs/design/gate-connection-admission-control.md:143（G4）、:207-215（P0-1）、:441（验收写的是 cat /proc/1/limits）。亲自打开看过
F11. 容量依据：c_gate 为 4.5k~20k。§7 把目标改为峰值在线 2000 万，没有改变单台的值，所以单台 gate 的 fd 需求是 20000 + 4096 = 24096，在线目标只影响台数 N_g（2000 万 ÷ 20k~4.5k = 1000~4444 台）。scene 到 SM/DS 的网状连接每个 scene 出连 4Z 条，S3 时为 400~892。按 §7.2 用 PCU 直接重算：Z = 2000 万 ÷ (45k~100k) ≈ 200~445，4Z ≈ 800~1780，属于推算
   证据: docs/design/capacity-model-hok-scale.md:46-48（c_gate/c_scene）、:148（0b 行）、:181（§1.3 第 2 条）、:197（gate 连接行）、:219（4Z 网状）、:405（A0-2）、:894-910（§7.2）。亲自打开看过
F12. C++ 自有代码里没有 select()、FD_SET，也没有 fork/exec/popen。把软限抬过 1024 之后，不会出现 FD_SETSIZE 越界或子进程继承大软限的问题。第三方里 FD_SET 只出现在 redis-server 的 ae_select 和 curl 的 select/multi；agones_rest_client 用的是 curl easy 接口，在 Linux 上走 poll（外部常识，未核实编译选项）
   证据: Grep 范围 cpp/（排除 generated）只命中 process_info.cpp:19 的一句注释；grep third_party/{redis,curl,librdkafka,muduo-linux}。亲自执行过
F13. tools/scripts/stress-linux-tier.sh 不启动 C++ 节点，它的「ulimit -n ≥ 1048576」只是对跑 robot 的那个 shell 的前置要求，本项不用改它
   证据: tools/scripts/stress-linux-tier.sh:1-60（:13）。亲自打开看过
F14. 测试工程的先例：client_endpoint_test 是 vcxproj，链接 core.lib、proto.lib 和 gtest，输出到 build/cpp/tests/。Linux 的 build_linux.sh 只构建固定的库和可执行工程清单，不构建测试，所以新增测试 vcxproj 不影响 Linux 构建
   证据: cpp/tests/client_endpoint_test/client_endpoint_test.vcxproj:24-27、:81、:124、:140、:164；game.sln:273-278、:563-567、:632；tools/scripts/build_linux.sh:88-111。亲自打开看过
F15. core.vcxproj 的头文件也逐个登记在 ClInclude 和 filters 里。build_linux.sh 从 vcxproj 生成 CMake，头文件不进源文件列表，所以新增一个只有头文件的模块，不需要改 CMakeLists.txt
   证据: cpp/libs/engine/core/core.vcxproj:405-416；core.vcxproj.filters:60-67；cpp/libs/engine/core/CMakeLists.txt:47-83；build_linux.sh:22、:246-256。亲自打开看过
F16. Go 服务用的是 Go 1.27，运行时启动时会自己把软限抬到硬限（Go 1.19 起的外部常识，未核实），不在本项范围内
   证据: go/login/go.mod:3（go 1.27.2）。亲自打开看过
F17. Java 版：镜像入口是 java -jar，部署只有 compose，没有 K8s 清单。game-gate 没有连接数上限配置，GateLimits 里没有这一项；game-battle 有 max-connections，缺省 4096。PARITY 的 P0-E 那一行标的是「Java 待做」
   证据: D:/luyuan/xuanming-server-mmo/deploy/docker/Dockerfile:63；game-gate/src/main/java/com/game/gate/session/GateLimits.java:18-19；docs/porting/inventory/gate.md:68；PARITY.md:186。亲自打开看过
## plan
- [cpp/libs/engine/core/node/system/node/open_file_limit.h（新建，只含头文件）] 纯判定，不做任何系统调用，可以在 Windows 上编译做单测。只依赖标准库加 proto/common/base/config.pb.h、node.pb.h，内容如下。

常量（每个都写清理由）：
- kUnlimited = UINT64_MAX
- kNonClientReserve = 4096：非客户端 fd 的余量，用于节点间 TCP/gRPC、Kafka/Redis/etcd、日志、每个 EventLoop 的 epoll/eventfd/timerfd，以及被拒连接在 forceClose 完成前仍占着的 fd。scene 在 2000 万在线时的网状连接 4Z 约 800~1780 条，留了约 2 倍以上的余量。这是推算，所以注释要写明这个余量只是「拒启的门槛」，进程真正可用的余量是抬起来之后的软限。
- kSoftLimitCeiling = 1048576：硬限是 infinity 或 1073741816 这类巨大值时，软限只抬到这里，避免有代码按 RLIMIT_NOFILE 分配数组或遍历所有 fd。

类型：
- struct Limits{uint64_t soft, hard;}
- struct ClientCap{uint64_t connections; std::string_view configKey;}：configKey 为空表示这类节点没有客户端面。

函数：
- ClientConnectionCapFor(uint32_t nodeType, const BaseDeployConfig&)：switch(static_cast<common::base::eNodeType>)，GateNodeService 返回 {gate_max_connections(), "GateMaxConnections"}，BattleNodeService 返回 {battle_max_connections(), "BattleMaxConnections"}，default 返回 {}。注释写明新增有客户端面的节点类型时要在这里加一个 case；配置为 0 只在 dev/test 出现，prod 由节点自己的门禁拒绝。
- constexpr RequiredOpenFiles(clientConnections) = clientConnections + kNonClientReserve。
- constexpr SoftLimitRaiseTarget(Limits cur, uint64_t required) = max(cur.soft, min(cur.hard, max(kSoftLimitCeiling, required)))，即永远不降低软限。
- enum class Verdict : uint8_t {kOk, kInsufficient}
- struct Outcome{Limits before; uint64_t required; uint64_t softAfter; bool raiseAttempted; int raiseErrno; Verdict verdict;}
- template<typename SetSoftLimitFn> Outcome Evaluate(Limits before, uint64_t required, SetSoftLimitFn&& setSoftLimit)：目标大于当前软限时才调一次 setSoftLimit(target)，返回 0 表示成功，否则返回 errno；成功时 softAfter = target，失败时保持原值并记下 errno；verdict = softAfter >= required ? kOk : kInsufficient。
- FormatStartupLine(role, cap, outcome)：固定格式，测试会锁定，例如 `[open_file_limit] role=gate verdict=ok required=24096 client_cap=20000 cap_key=GateMaxConnections reserve=4096 soft_before=1024 soft=524288 hard=524288 raise=ok`。无界的值写 unlimited；raise 取 none / ok / failed(errno=N)；scene 写 cap_key=none。
- FormatRefusalReason(role, cap, outcome)：英文，内容依次是谁、差多少、硬限只能在节点运行时层面改（containerd/dockerd 的 systemd LimitNOFILE，或 CRI base_runtime_spec 的 rlimits；K8s 没有按容器的 ulimit 字段）、或者调低某个配置键，最后指向 deploy/k8s/README.md 的对应一节。

不用宏，不复制第二份业务真相。
   工作区: 文件不存在（find cpp -name 'open_file_limit*' 无结果），不存在冲突
- [cpp/libs/engine/core/node/system/node/node.cpp] 1）include：加 "node/system/node/open_file_limit.h"；在 `#ifdef __linux__` 里加 <sys/resource.h>。<cerrno> 和 <cstdio> 已经有了（:43-45）。

2）在匿名命名空间里、FatalStartupConfig（:366-372）之后，新增 `void EnsureOpenFileLimitOrDie(uint32_t nodeType)`：
- Linux 下：cap = ClientConnectionCapFor(nodeType, tlsNodeConfigManager.GetBaseDeployConfig())，required = RequiredOpenFiles(cap.connections)。
- 调 ::getrlimit(RLIMIT_NOFILE)；失败就 FatalStartupConfig("open file limit", "getrlimit failed errno=…")，无法核实即 fail-closed。
- 把 RLIM_INFINITY 映射成 kUnlimited，然后调 Evaluate，回调里拷贝 current，只改 rlim_cur 后 ::setrlimit，硬限原样带回。
- role 取 NodeUtils::NodeTypeToShortName(nodeType)。
- 无论结果如何，都把 FormatStartupLine 写到 stdout 并 fflush，写法同 gate_version::PrintStartupLine。K8s 上 kubectl logs 唯一能看到它的地方就是 stdout。
- verdict 为 kInsufficient 时，调 FatalStartupConfig("open file limit", FormatRefusalReason(…))，先写 stderr 再 LOG_FATAL；否则 raiseErrno != 0 时打 LOG_WARN，正常时打 LOG_INFO（进日志文件，再由 sidecar 送到 Loki）。
- 非 Linux 下：LOG_DEBUG 一行 skipped 后直接返回。

3）在 Node::Initialize 的 InitLogSystem()（:513）和 grpc_call_deadline::Apply（:516）之间调用 EnsureOpenFileLimitOrDie(GetNodeType())。注释写清：这里是第一个配置已读取、又还没有任何网络 fd 的位置；三类 C++ 节点都经过这里，以后新增的节点也自动覆盖；和运行模式无关的理由；拒启的理由（fail-closed，见交付说明）。

不改 node.h，不改 gate、scene、battle 的 main.cpp。gate 和 battle 的 main.cpp 有别的会话未提交的改动，这样可以避开冲突。
   工作区: git status 干净；mtime 2026-10-15 22:09:37。这是热点文件，动手前用 git status 和 git log -1 再确认一遍有没有新的快照提交
- [cpp/libs/engine/core/core.vcxproj 与 core.vcxproj.filters] 在 node\system\node\ 那一组 ClInclude 里追加 `<ClInclude Include="node\system\node\open_file_limit.h" />`：vcxproj 在 :409 附近，filters 在 :66 附近，照 client_endpoint.h 的写法。只追加，不重排。
   工作区: git status 干净；两个文件 mtime 都是 2026-10-15 22:11:27
- [cpp/tests/open_file_limit_test/open_file_limit_test.cpp（新建）] gtest，只测 open_file_limit.h 里的纯函数；系统调用通过 Evaluate 的回调注入假实现（记录调用次数和参数），不依赖墙钟和真实进程限值。用例见 tests 字段。
   工作区: 目录不存在
- [cpp/tests/open_file_limit_test/open_file_limit_test.vcxproj（新建）] 复制 cpp/tests/client_endpoint_test/client_endpoint_test.vcxproj，然后改四处：新的 ProjectGuid、RootNamespace/ProjectName 改成 open_file_limit_test、ClCompile 改成 open_file_limit_test.cpp。include 目录（包含 ../../libs/engine/core/ 和 ../../generated/）和依赖（core.lib、proto.lib、gtest、protobuf、absl）原样保留，因为 BaseDeployConfig 需要链接 proto.lib。不建 filters，先例里也没有。
   工作区: 不存在
- [game.sln] 新增 Project 块，ProjectDependencies 照 :273-278 写两条：{03F2DF47-…} 和 {7D732139-…}。在 ProjectConfigurationPlatforms 里照 :563-567 加 5 行，在 NestedProjects 里照 :632 挂到 {BD08644C-5088-49D6-A48E-8D8B916D073B} 下面。只追加，不重排，不动别的会话加的依赖行。
   工作区:  M；mtime 2026-10-15 22:48:53；未提交 22 行，内容是给多个测试补 ProjectDependencies，和本项无关。有冲突风险（中）：编辑前重新读一遍，只追加
- [bin/etc/base_deploy_config.yaml] 只改注释。:134-135 那句「并确保不超过进程的 fd 上限(ulimit -n …)」改为：Linux 上节点启动时会把 RLIMIT_NOFILE 软限抬到硬限（最多 1048576），抬完仍小于本值 + 4096 就拒绝启动（见 cpp/libs/engine/core/node/system/node/open_file_limit.h）；硬限由节点上的容器运行时或 systemd 决定，K8s 清单里声明不了。BattleMaxConnections（:147-149）的注释补一句同样的话。不动任何键和值，所以 k8s_deploy 镜像出来的 ConfigMap 不变。
   工作区:  M；mtime 2026-10-15 23:29:04；别的会话未提交的改动只有 :52 加了一个空行、:97 改了一处注释路径，和本项无关；冲突风险低，只改 :134-135、:147-149 的注释
- [deploy/k8s/README.md] 在「## C++ 节点的 zone Redis 口令」（:1415）这一节后面新增一节「## C++ 节点的文件描述符上限（A0-2）」，内容包括：
- 进程自己抬软限、拒启的条件和需求值（gate 24096、battle 8192、scene 4096）；
- 为什么不在清单里做（没有 ulimit 字段；initContainer 改不了别的进程；特权 prlimit 已被否决；启动壳的 ulimit 和进程自抬是同一件事，不做两份）；
- 硬限不够时怎么办：节点上 containerd 加 drop-in `LimitNOFILE=1048576` 后重启 containerd（只影响之后新建的容器），或者在 CRI 的 base_runtime_spec 里写 rlimits，或者调低准入上限。这些都标注为外部常识，待实测；
- 怎么验收：启动行用 kubectl logs 加 grep `[open_file_limit]` 查看；进程限值要按 /proc/*/comm 找到 gate/scene/battle 的 pid 再读它的 limits，gate/scene 的 PID 1 可能是 sh，不要读 /proc/1/limits。
   工作区:  M；mtime 2026-10-16 03:53:20，正有别的会话在改（+72/-23，集中在 MySQL 和 Redis 总账两节）。冲突风险中，只在 :1415 之后插入新的一节
- [docs/design/gate-connection-admission-control.md] 在 G4 那一行所在表格的后面，以及 §6 P0-1（:207-215）下面，各追加一条带日期的落码说明：已落码，未编译未验证；位置在 Node::Initialize；需求的算法；和运行模式无关。另外订正 :441 的验收方法：gate/scene 的 PID 1 可能是 sh，应按 pid 读 limits。原文不删。
   工作区: git status 干净；mtime 2026-10-12 03:53:02
- [docs/design/capacity-model-hok-scale.md] 在 §1.3 第 2 条（:181）、A0-2（:405）、§2.1 gate 那一行（:197，「K8s 上进程从不 setrlimit」）后面各追加带日期的括注：已落码，未验证，阈值是准入上限 + 4096，见 PROGRESS 条目。不改数字。
   工作区: git status 干净；mtime 2026-10-15 14:38:41
- [cpp/nodes/gate/SECURITY.md] 在 :257「① 进程从不 setrlimit…」后面追加一条带日期的更正，说明已改为在 Node::Initialize 里自抬软限、不够就拒启，并指向设计文档。
   工作区: git status 干净；mtime 2026-10-15 10:49:16
- [docs/ops/gate-kernel-tuning-runbook.md] §三「进程级 ulimit」（:79-104）补一段「K8s / 容器」：C++ 进程启动时自己抬软限；硬限必须不低于需求值；改硬限的方法；验收命令。原有的 limits.d 和 systemd 写法保留，裸机仍然适用。
   工作区: git status 干净；mtime 2026-10-12 03:53:02
- [docs/PROGRESS.md] 在末尾追加一条（文件按 merge=union 合并，只追加，不改旧条目）。写清问题、做法、改了哪些文件；全部未编译、未测试；交 Codex 的命令；Java 版状态为待做（0.1.0-SNAPSHOT，调查时 HEAD 76e7bad）；尚未实测目标集群的硬限（E11）。
   工作区:  M；mtime 2026-10-16 04:04:43，正有别的会话在追加（+252 行）。union 合并，只追加，风险低
- [docs/handoff/handoff-launch-readiness-and-d1-20261013.md] 在文首进度段后面追加一行带日期的说明：A0-2 的 RLIMIT_NOFILE 已落码、未验证，范围覆盖 gate/scene/battle，并指向 PROGRESS 条目。正文不改。
   工作区:  M；mtime 2026-10-15 22:15:56；未提交 2 行，是文首的「10-15 晚，续」，和本项无关；只追加
- [D:/luyuan/xuanming-server-mmo/PARITY.md（Java 仓）] 在末尾追加一行。功能：C++ 节点 RLIMIT_NOFILE 自抬与启动断言；位置：open_file_limit.h 和 node.cpp 的 Node::Initialize；commit：未提交；Java 模块：game-gate、game-scene、game-battle；版本：0.1.0-SNAPSHOT；状态：Java 待做。备注写清：JVM 在 Linux 上缺省开启 -XX:+MaxFDLimit，会自动把软限抬到硬限（外部常识，未核实），所以缺的只有启动断言；做法建议见 java_parity 字段。
   工作区:  M（Java 仓）；mtime 2026-10-16 03:32:52；未提交 +14/-2，是别的会话的 P0-E 那一行；只追加
## compat_and_rollout
- **不涉及任何契约或格式**：不改 proto、配置键、K8s 模板、ConfigMap，也不改客户端契约。k8s_deploy 渲染结果逐字不变，k8s_deploy_contract 不受影响。新旧版本之间没有线上交互的差异，滚动升级期间混跑安全。回滚就是换回旧镜像，没有数据迁移。
- **行为变化只在 Linux**：进程启动时把软限抬到 min(硬限, 1048576)；硬限低于需求（gate 24096、battle 8192、scene 4096）时拒启，表现为 CrashLoop，stderr 能看到 `FATAL open file limit config:`。Windows 本地什么也不做。

**为什么拒启而不是告警**（按 AGENTS §11.3 取舍）：
- 一台 gate 在硬限低于准入上限时照样启动，login 仍会按 player_count 小于上限持续往它派人。fd 耗尽后，唯一的 IO 线程会陷入 EMFILE 忙循环并刷日志（G7），在线玩家一起卡住；scene 里 Kafka、Redis、etcd 重连也会失败，存盘和会话的一致性面临风险。这比起不来更糟。
- 和 Redis 不同（单实例，起不来就全服停，所以 P0-E 有意 fail-open），这三类节点都是多副本：gate 的 Deployment 2 副本时 maxSurge 为 1、maxUnavailable 为 0，新 Pod 崩溃只会让滚动卡住，旧 Pod 继续服务。
- 不学 Redis 自动下调 maxclients：那会让准入上限悄悄失真，而 scene 本来也没有上限可以下调。
- 和运行模式脱钩：scene 的运行模式同时决定 GM 指令放不放行，不能让运维为了绕过 fd 检查而去打开 GM。

**滚动风险按形态**：
- Deployment（podip 形态的 gate、Deployment 模式的 scene、验证用的 battle Deployment）：滚动卡住，旧 Pod 保留。
- gate StatefulSet：OnDelete 加 k8s_gate_drain，一次只排空一台；新 Pod 起不来时那一台就空着。
- battle hostPort：maxSurge 0、maxUnavailable 1，最多少一台。
- Agones Fleet（scene/battle）：GameServer 退出后变 Unhealthy 并被替换，会反复重建，最多按 RollingUpdate 缺省的 25% 少容量，需要盯 `kubectl get gs`。

**上线顺序**：
1. 先在 kind 和每个目标集群的每个节点池上，从现有的旧 Pod 里读一次硬限，这就是 E11 的证据：gate 需要 24096 以上，battle 8192 以上，scene 4096 以上。
2. 不够的先改节点运行时，或者调低 GateMaxConnections / BattleMaxConnections 并增加台数。
3. 再发新镜像。staging/prod 的硬限由谁去读，见 open_decisions。
## tests
- 新建 cpp/tests/open_file_limit_test（gtest，Windows MSBuild）。全部是纯函数，通过 Evaluate 的回调注入假的系统调用，不依赖墙钟、不读真实 rlimit。新头文件落地之前这些用例编译不过（这就是红），落地后转绿。用例如下：
-   RequiredForGateIsMaxConnectionsPlusReserve：BaseDeployConfig.gate_max_connections=20000、节点类型 GateNodeService → ClientConnectionCapFor 返回 {20000, "GateMaxConnections"}，Required = 24096
-   RequiredForBattleUsesBattleMaxConnections：battle_max_connections=4096 → 8192，cap_key=BattleMaxConnections；同一份配置里 gate_max_connections=20000 不会被误用到 battle 上
-   RequiredForSceneIsReserveOnly：SceneNodeService 即使配置里带着 GateMaxConnections=20000 → 4096，configKey 为空
-   DisabledCapNeedsReserveOnly：gate_max_connections=0（dev/test）→ 4096
-   RaiseTargetIsHardWhenBelowCeiling：{1024,524288} → 524288；{1024,1024} → 1024
-   RaiseTargetCappedAtCeiling：{1024,1073741816} 与 {1024,kUnlimited} → 1048576
-   RaiseTargetNeverLowersSoft：{2000000,2000000} → 2000000；{kUnlimited,kUnlimited} → kUnlimited
-   EvaluateRaisesSoftToHardAndPasses：before {1024,524288}，required 24096 → 回调恰好被调 1 次、参数 524288，softAfter=524288，raiseAttempted=true，errno=0，kOk
-   EvaluateSkipsSyscallWhenAlreadyAtTarget：{65536,65536}，required 24096 → 回调调用 0 次，kOk
-   EvaluateRefusesWhenHardBelowRequired：{1024,1024} → 回调 0 次，kInsufficient；{1024,8192}，required 24096 → 回调 1 次（8192），softAfter=8192，kInsufficient
-   EvaluateRefusesWhenRaiseFails：{1024,524288}，回调返回 1（EPERM）→ softAfter=1024，raiseErrno=1，kInsufficient
-   EvaluatePassesWhenRaiseFailsButSoftSufficient：{30000,524288}，回调返回 1 → kOk，raiseErrno=1（用于产出 WARN 行）
-   BoundaryExactlyRequiredPasses：{24096,24096} → kOk；{24095,24095} → kInsufficient
-   StartupLineFormatIsStable：逐字锁定 gate 那一行和 scene 那一行（cap_key=none）的格式；kUnlimited 渲染为 unlimited；raise 字段的三种取值。这一行是给运维和压测 grep 的契约
-   RefusalReasonNamesKnobs：拒启文案里要包含 role、required、cap_key 与它的取值，以及 hard limit 和 container runtime 两个关键词
- 容器级红绿对比（由 Codex 执行，见 verify_commands 第 3 步）。同一组 --ulimit 用例分别在旧镜像（改动前）和新镜像上跑：旧镜像在 1024:1024 下不会拒启，在 1024:524288 下软限仍是 1024，这是红；新镜像拒启、或软限被抬到 524288，这是绿
- kind 实测（由 Codex 执行）：升级前从旧 Pod 读出硬限（E11 证据），升级后三类节点都有 verdict=ok 的启动行，且进程的软限等于 min(硬限, 1048576)
- 回归：client_endpoint_test、configuration_table_test 重新编译并跑一遍；gate、scene、battle 三个 vcxproj 能编译；Windows 本地按平常方式起节点，行为不变
## verify_commands
- 【第 1 步，Windows MSBuild，按顺序串行，每条都带 /m:1】工作目录是仓库根 D:\luyuan\xuanming-server-mmo-cpp，在 VS 开发者 PowerShell 里执行。前提是 proto.lib 已经构建过。依次执行：`msbuild cpp\libs\engine\core\core.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` → `msbuild cpp\tests\open_file_limit_test\open_file_limit_test.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` → `build\cpp\tests\open_file_limit_test.exe`。通过标准：0 个编译错误，gtest 全部 PASS，退出码 0。失败时保留 msbuild 的前 30 条 error 和 gtest 的失败断言原文
- 【第 2 步，C++ 节点编译回归】同一工作目录，继续串行：`msbuild cpp\nodes\gate\gate.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`，scene（cpp\nodes\scene\scene.vcxproj）和 battle（cpp\nodes\battle\battle.vcxproj）同样各跑一次；然后重新编译并运行 build\cpp\tests\client_endpoint_test.exe 和 configuration_table_test。通过标准：都是 0 error，测试全绿。用户自己编译时同样按这个顺序
- 【第 3 步，Linux 镜像加容器级红绿对比】在仓库根执行 `docker --context desktop-linux build -f deploy/k8s/Dockerfile.cpp -t mmorpg-node:nofile-new .`。旧镜像优先用本机已有的、改动前的 ghcr.io/luyuancpp/mmorpg-node:<tag>（`docker images` 查找），没有的话在 HEAD 的临时 git worktree 里用同一条命令构建出 `mmorpg-node:nofile-old`。下面命令里统一设 $etc='D:/luyuan/xuanming-server-mmo-cpp/bin/etc'，公共参数为 `-v "${etc}:/app/bin/etc:ro" -w /app/bin`：
(a) 拒启：`docker --context desktop-linux run --rm --ulimit nofile=1024:1024 <公共参数> mmorpg-node:nofile-new ./gate`。期望 30 秒内以非 0 退出（预计 134），stderr 里有 `FATAL open file limit config:` 和 `required=24096`，并且没有 verdict=ok 那一行。
(b) 自抬：`docker ... run -d --name nf-gate --ulimit nofile=1024:524288 <公共参数> mmorpg-node:nofile-new ./gate`。最多轮询 30 秒，直到 `docker logs nf-gate` 里出现 `[open_file_limit] role=gate verdict=ok required=24096 client_cap=20000 cap_key=GateMaxConnections reserve=4096 soft_before=1024 soft=524288 hard=524288 raise=ok`。出现后立刻执行 `docker exec nf-gate sh -c 'grep "Max open files" /proc/1/limits'`，期望 524288 524288（docker run 时 PID 1 就是 gate），最后 `docker rm -f nf-gate`。进程之后因为连不上基础设施而报错，可以忽略。
(c) scene 边界：`--ulimit nofile=4095:4095` 跑 ./scene 期望拒启；`4096:4096` 期望 verdict=ok 且 raise=none。
(d) battle 边界：`8191:8191` 跑 ./battle 期望拒启；`1024:8192` 期望 verdict=ok 且 soft=8192。
(e) 旧镜像对照：同样的 (a)、(b) 换成 nofile-old 跑。期望 (a) 30 秒内不退出、也没有 FATAL open file limit，(b) 的 /proc/1/limits 软限仍是 1024 且没有 [open_file_limit] 行，这就是修复前的红。
每个用例保留前 40 行输出和退出码。如果在 [open_file_limit] 那一行之前进程就因为别的原因 FATAL 了，记下原文并停下归因，不要连续重试
- 【第 4 步，kind 实测】升级前，对每个 zone 命名空间里现有的 gate、scene、battle Pod 各执行一次（battle 在全局池的命名空间里）：`kubectl -n <ns> exec <pod> -c <容器名> -- sh -c 'for p in /proc/[0-9]*; do n=$(cat $p/comm 2>/dev/null); case "$n" in gate|scene|battle) echo $n; grep "Max open files" $p/limits;; esac; done'`，记录 soft 和 hard，作为 E11 的证据。然后按平常流程用新镜像跑 zone-up（dev 档，`pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up …`，参数和平常一样），再执行 `kubectl -n <ns> logs <pod> -c <容器名> | Select-String '\[open_file_limit\]'`，并重复上面的 exec。通过标准：三类 Pod 都 Ready；每类都有 verdict=ok 的行；进程软限等于 min(旧 hard, 1048576)。如果 hard 低于需求，Pod 会 CrashLoop，这时记下 stderr 的 FATAL 行和节点上 containerd 的版本，停下等决策
- 【第 5 步，文档索引】在仓库根执行 `python tools/scripts/gen_docs_index.py --check`，退出码应为 0（本项不新增文档，只是确认没有破坏索引）
## doc_updates
- docs/PROGRESS.md 末尾追加条目：做法、文件、未编译未测试、Codex 命令、Java 待做（0.1.0-SNAPSHOT，HEAD 76e7bad）、E11 尚未实测
- deploy/k8s/README.md 新增一节「C++ 节点的文件描述符上限（A0-2）」：需求值、为什么不在清单里做、硬限不够时的补救（containerd drop-in LimitNOFILE / CRI base_runtime_spec / 调低准入上限）、按 pid 读 limits 的验收方法
- docs/design/gate-connection-admission-control.md：在 G4 和 P0-1 后面追加落码说明；订正 :441 的验收方法（gate/scene 的 PID 1 可能是 sh）
- docs/design/capacity-model-hok-scale.md：在 §1.3 第 2 条、A0-2、§2.1 gate 行后面加括注「已落码、未验证、阈值是准入上限 + 4096」
- cpp/nodes/gate/SECURITY.md:257 ①：追加更正
- docs/ops/gate-kernel-tuning-runbook.md §三：补「K8s / 容器」一段
- bin/etc/base_deploy_config.yaml：GateMaxConnections 和 BattleMaxConnections 的注释改为说明进程会自抬软限、需求值是多少、不够就拒启
- docs/handoff/handoff-launch-readiness-and-d1-20261013.md：在文首进度段追加一行状态
- Java 仓 PARITY.md：在末尾登记一行「Java 待做」
- 不改 docs/ops/release-checklist.md：A.4 在 2026-05 的历史附录里（:188 起），不往历史清单里加新要求。不新增文档，所以 docs/README.md 不用改
## java_parity
Java 版状态是待做，不需要和本仓同批完成。本项属于服务端内部的运维与启动门禁，不碰客户端契约（帧格式、proto、tip、配置表都不变）。按 AGENTS §12，Java 用自己惯用的方式实现，本批只在 PARITY 登记。

为什么只缺一半：Java 进程跑在 HotSpot 上，Linux 下缺省开启 -XX:+MaxFDLimit，启动时会把软限抬到硬限（外部常识，两个仓都未核实）。所以「自抬」这一半 JVM 已经替 Java 做了，缺的是「启动断言」。

建议做法：
- game-battle：在现有的启动门禁（BattleConfiguration / BattleProperties.requireAllowedIn）旁边，读 `((com.sun.management.UnixOperatingSystemMXBean) ManagementFactory.getOperatingSystemMXBean()).getMaxFileDescriptorCount()`，与 effectiveMaxConnections + 4096 比较；prod 下不够就拒启，并打一行 fd 限值。
- game-gate：Java 版没有连接数上限配置（GateLimits.java:18-19 里没有；docs/porting/inventory/gate.md:468 还是悬而未决），先只做余量 4096 的断言，等 gate 的连接上限定下来再补。
- game-scene：只做余量断言。
- 部署：Java 只有 compose，可以在 deploy/compose 的服务上用 `ulimits: nofile` 显式给硬限，这是 Docker 层的能力，本仓的 C++ 没有 compose 部署。

另外两条：Java 版本号是 0.1.0-SNAPSHOT，调查时 HEAD 为 76e7bad。本仓的 Go 服务是 Go 1.27，运行时会自己抬软限（外部常识），不在本项范围内。
## risks
- 目标集群的硬限还没有实测（交接里的 E11）。哪个节点池的 containerd/dockerd 给的硬限低于 24096（gate）、8192（battle）或 4096（scene），新 Pod 就会拒启：Deployment 会滚动卡住，gate StatefulSet 会空出一台，Agones Fleet 会反复重建 GameServer，最多少 25% 的容量。必须先按 verify_commands 第 4 步用旧 Pod 读出硬限
- 外部常识都还没有在本仓核实：containerd 2.x 或 Docker 25 以上配 systemd 240 以上时是 1024/524288，containerd 1.x 的 unit 写的是 infinity；CRI 不设 rlimits，所以容器继承运行时的限值；JVM 的 MaxFDLimit；Go 1.19 起自抬软限；curl 在 Linux 上走 poll；kindest/node v1.35.8 里的实际值。都需要 Codex 实测
- 余量 4096 是推算值（[I]）。2000 万在线时，scene 的网状连接 4Z 约 800~1780 条，前提是每个 zone 的 SM/DS 各 2 副本、A1 控制面还没按 zone 化。如果 Z 超过约 900，或副本数变多，4096 就不足以作为「保证」。不过进程实际可用的上限是抬起来之后的软限（通常 524288），所以只是拒启门槛偏低，不会提前把进程掐死。到时候可以改常数，或者把它改成配置键
- 引擎里的检查比 gate 自己的 ValidateGateConnectionLimitOrDie 先跑。如果误配了 GateMaxConnections > 131071，又碰上硬限不够，先看到的会是 fd 拒启，而不是 gate 的上限报错。拒启文案里带着配置键和取值，可以诊断，只是两个问题的报错先后会变
- 附带发现，按规定不在本项里修：gate 和 battle 现有的 LOG_FATAL 启动门禁（空密钥、上限为 0 等），在 Linux 上因为异步日志加 abort，可能在 kubectl logs 里一点痕迹都不留。本项的拒启走 stderr，不受影响。建议另开一项，让那些门禁也统一改走 FatalStartupConfig
- gate/scene 的启动命令不是 exec，PID 1 可能是 sh（dash 的 exec 优化未核实）。设计文档 :441 用 /proc/1/limits 验收会读到 sh 的软限，会误判；验收一律按 pid 去读
- 工作区冲突。game.sln、bin/etc/base_deploy_config.yaml、deploy/k8s/README.md（03:53 还在被改）、docs/PROGRESS.md（04:04）、交接文档、Java 仓 PARITY.md 都有别的会话未提交的改动，而且可能被整区快照提交。本项对这些文件只做局部追加，动手前重新看 git status 和 git diff。node.cpp 目前干净但是热点文件
- 不处理全系统层面的 fs.file-max / ENFILE（G6：sockets::accept 遇到 ENFILE 直接 LOG_FATAL），也不做运行期 fd 使用率的观测（C++ 节点没有 Prometheus 出口，G13）。都属于后续事项
- Linux 上的系统调用路径在 Windows 单测里覆盖不到，只能靠第 3 步的容器实验和第 4 步的 kind 实测来验证
## open_decisions
- 预发和生产集群上，各节点池容器的 nofile 硬限（E11，以及全局单点 P0 交接里同一个待办）由谁去读一次？读法见 verify_commands 第 4 步的 exec 命令。如果哪个池不到 24096（gate）、8192（battle）或 4096（scene），由谁去改节点上 containerd/dockerd 的 LimitNOFILE？这一步改的是集群运维配置，不在本仓，也超出 Codex 能操作的环境

# VERDICT
## overall
核心事实基本成立，方向正确。所有 C++ 节点都经过 Node::Initialize；插入点 :513 与 :516 之间没有网络 fd；复用 FatalStartupConfig 走 stderr；按 Linux 条件编译；不改 proto、配置键和 K8s 模板，滚动升级期间新旧版本混跑安全；拒启的取舍也有理由。我查到的问题都不推翻方案，但有几处遗漏和事实错误，需要在落码前补正。①裸 Linux 的 deploy-staging.sh 启动路径被漏掉：新的 fail-closed 门槛会让这条路径静默失败。②新测试工程没有登记到 run_cpp_tests.ps1。③Java 对等里「部署只有 compose、给 compose 服务加 ulimits」是错的：Java 的 compose 只有依赖，游戏进程在宿主上由 start-slice.sh 启动，所以 PARITY 备注要改写。④上线次序（E11 先测硬限）全靠人工约定，又会被整区快照自动带进镜像，需要把它写成硬前置并给出回退命令，或者经负责人确认后加一个与运行模式无关的运维豁免开关。⑤第 3 步容器级红绿判据和「预计 134」不可靠。其余是余量论证没算 gate 到全服 SM 的约 2Z 条连接、RLIM_INFINITY 边角情况，以及工作区 diffstat 误读等小问题。规格声称「未做」属实：工作区和 HEAD 都没有 getrlimit/setrlimit 落码，PROGRESS 也没有落码条目。整个核实过程只读，没有改动或创建任何文件，也没有执行编译或测试。
## fact_checks
- [confirmed] 三类节点在类型构造函数里先 set_node_type 再调 Initialize；Initialize 顺序为 LoadConfigs → InitLogSystem → grpc_call_deadline::Apply → InitRpcServer（连 Redis）→ LoadAllConfigData → InitKafka → InitEtcdService，:513 与 :516 之间是第一个读到配置、又还没开网络 fd 的位置
   亲自打开 node.cpp:418-430（:428 set_node_type，:430 Initialize）、:497-521（:512 LoadConfigs、:513 InitLogSystem、:516 Apply、:517 InitRpcServer、:519 InitKafka、:520 InitEtcdService）。另核 InitRpcServer 正文 :524-590：InitClientEndpoint（agones 模式会走 SDK HTTP）与 tlsRedis.Connect 都在 :517 之内，插入点之前确实没有网络 fd（只有 EventLoop 自身的 epoll/eventfd/timerfd 与异步日志文件）。
- [confirmed] gate/battle 走 RunSimpleNodeMainWithOwnedContext、先构造 Node 再 configure；scene main 直接构造 Node；ValidateGateConnectionLimitOrDie 在 configure 里，晚于 Initialize
   亲自打开 node_entry.h:174-193（:187 构造 Node、:190 configure）；scene/main.cpp:86-106；gate/main.cpp:97-138 定义、:243 调用（工作区版本）；battle/main.cpp:109 定义、:281 调用 ValidateBattleClientEdgeConfigOrDie。
- [confirmed] Linux 上 muduo 日志只进异步文件；LOG_FATAL 后 g_flush 只冲 stdout 即 abort，FATAL 很可能落不了盘；FatalStartupConfig 先 fprintf(stderr)+fflush 再 LOG_FATAL，可复用
   亲自打开 node.cpp:1367-1377（AsyncOutput 仅 #ifdef WIN32 调 LogToConsole）、:399 setOutput(AsyncOutput)、全文件只有 :399/:1253 两处 setOutput、无 setFlush；node.cpp:366-372 FatalStartupConfig；third_party/muduo-linux/muduo/base/Logging.cc:196-205（~Logger 遇 FATAL g_flush()+abort()）。补充：Linux 镜像用的 overlay（tools/archived/muduo_linux_overlay）不含 Logging.cc；node.cpp:223-251 的 HandleFatalSignal 也不冲异步日志，结论成立。
- [confirmed] GateMaxConnections=20000、BattleMaxConnections=4096；ConfigMap 原样镜像；battle 部署时校验 1..65535
   亲自打开 bin/etc/base_deploy_config.yaml:125-139、:147-149；config.cpp:105-118。k8s_deploy.ps1 正被别的会话改（mtime 10-16 04:10:20），行号已漂移：GateMaxConnections 读取现在 :3156，battle 1..65535 校验现在 :3215-3220，模板 :3270 附近；内容与调研者描述一致。battleSettingsBlock 只在 $IncludeBattleSettings 时出现，zone ConfigMap 里没有 BattleMaxConnections（对本方案无影响，方案按节点类型取键）。
- [confirmed] 值为 0 时 gate 回落 131071、battle 回落 65535；prod 由节点门禁拒绝
   亲自打开 gate/main.cpp:106-125；client_message_processor.cpp:629-636（effectiveMax = 0 ? kSeqMask）；battle_client_edge.h:57；battle_client_edge.cpp:163-165；battle/main.cpp:177-180（工作区版本）。
- [confirmed] K8s 所有启动形态都是 /bin/sh -lc 外壳、无 ulimit；gate/scene 不 exec、battle exec；PID 1 可能是 sh
   亲自打开 k8s_deploy.ps1:3558-3559（New-NodeDeploymentYaml，现行行号）、:4091-4092（Fleet）、:7984/:7994/:8006（./gate、./scene）、:8298-8309（exec ./battle）；k8s_client_entry.ps1:700-706；:338 的 CppLogPrunerCommand 是 `(while ...; done &) && ` 后台子壳。dash 是否对最后一条命令做 exec 优化仍未核实，PID 1 是谁属 unverifiable，调研者已如实标注。
- [confirmed] C++ 工作负载不在 deploy/k8s/manifests/** 下；镜像 USER 10001、无 ENTRYPOINT/ulimit
   亲自 ls deploy/k8s/manifests/{go-svc,infra,java-svc}，无 gate/scene/battle；grep Dockerfile.cpp 只有 :279 USER 10001:10001、无 ENTRYPOINT/CMD/ulimit；Dockerfile.runtime:74 同。
- [confirmed] K8s 不注入 GATE/SCENE/BATTLE_RUN_MODE；scene 运行模式同时决定 GM 放行
   亲自打开 k8s_deploy.ps1 现行 :3186-3190、:3207（行号较调研者所引后移约 3 行）；player_gm_guard.h:31-44（SCENE_RUN_MODE 与 GM 判定同源）。
- [confirmed] kafka.yaml 在 exec 前 ulimit -n "$hard"；redis.yaml 只打印、硬限不够只报 ERROR 不拒启
   亲自打开 kafka.yaml:166-185、redis.yaml:99-133（MAXCLIENTS=40000）。
- [confirmed] G4 判定 RLIMIT_NOFILE 全链路无人设；P0-1 要求软限不足拒启并抬到硬限，不走 initContainer+privileged；验收写 cat /proc/1/limits
   亲自打开 gate-connection-admission-control.md:143、:207-215、:441。注意 :212 原文写的是「参照 Redis 的做法（抬不上去就下调 maxclients 并打日志）」，规格选择不下调、直接拒启，属于对设计稿的有意偏离，理由在 compat_and_rollout 里写了，落码时须在设计文档里同步记录这次偏离。
- [confirmed] 单台 gate fd 需求 20000+4096=24096；scene 网状 4Z≈800~1780（推算）；摘要称 scene「同样超过 1024」
   亲自打开 capacity-model-hok-scale.md:46-49、:148、:181、:197、:219、:405、:894-905，数字可复算（2000 万 ÷ 100k=200 → 4Z=800；÷45k≈444 → 1778）。两处订正：①摘要「同样超过 1024」只对上半区间成立，C_z=100k 时 4Z=800 < 1024；②gate 白名单含 SceneManagerNodeService（gate/main.cpp:232-233），而 IsZoneScopedNodeType（node_util.cpp:115-131）不含 SM，故直连模式的 gate 自身也有约 2Z≈400~890 条到全服 SM 的连接，余量论证只写了 scene。这一点是推断，未逐行核实 node_connector 的建连逻辑。
- [confirmed] C++ 自有代码无 select/FD_SET/fork/exec/popen；agones_rest_client 用 curl easy 接口，在 Linux 上走 poll
   我亲自 Grep 了 cpp/（排除 generated）：只命中注释、muduo_windows 适配层和 ProcessInfo.cc。agones_rest_client.cpp:139、:151 确为 curl_easy_*。补充：Linux 镜像链接的是发行版 libcurl（Dockerfile.cpp:47 libcurl4-openssl-dev、:219 libcurl4），不是 third_party/curl，所以 grep third_party/curl 证明不了 Linux 二进制的行为。发行版 libcurl 走 poll 属外部常识，未核实。
- [confirmed] stress-linux-tier.sh 不启动 C++ 节点，本项不用改
   亲自打开 stress-linux-tier.sh:1-40，该脚本只跑 robot。漏掉的是同目录的 tools/scripts/deploy-staging.sh:132-146：它在裸 Linux 上直接后台启动 bin/scene 和 bin/gate，没有设 GATE_RUN_MODE，也不等端口监听，最后照样打印 all services up（:160）。docs/ops/linux-staging-stress-runbook.md:20-28 是对应的手册。
- [confirmed] 测试工程先例 client_endpoint_test；build_linux.sh 不构建测试
   亲自打开 client_endpoint_test.vcxproj:24-27、:81、:124、:140、:164；game.sln:273-278、:563-567、:632；build_linux.sh:88-111。漏项：tools/scripts/run_cpp_tests.ps1:45-73 是 C++ 测试的统一入口，靠 $projects 有序表登记（:48 client_endpoint_test）。脚本头注释 :6-8 说明：不在表里的工程会「悄悄烂掉」。
- [confirmed] core.vcxproj 头文件逐个登记；CMakeLists 由 vcxproj 生成，纯头文件不需改 CMake
   亲自打开 core.vcxproj:403-418、core.vcxproj.filters:58-68、core/CMakeLists.txt:47-85（只列 .cpp）、build_linux.sh:244-258。
- [unverifiable] Go 1.27，运行时自动把软限抬到硬限
   go/login/go.mod:3 为 go 1.27.2，已亲自打开确认；运行时自抬属外部常识，仓内无法核实（调研者已标注）。
- [refuted] Java 版：镜像入口 java -jar；部署只有 compose、无 K8s；gate 无连接上限；battle max-connections 缺省 4096；PARITY P0-E 行为 Java 待做
   亲自打开。属实的部分：Dockerfile:63 ENTRYPOINT java -jar；GateLimits.java:18-19 没有连接上限；gate.md:68 与 :468；BattleProperties.java:63-67、:113、:122-128；PARITY.md:186。不实的部分：「部署只有 compose」。deploy/compose/infra.yaml:11、:22-24 写明 compose 只放 MySQL/Redis/Kafka 依赖，游戏进程由 tools/local/start-slice.sh:284 在宿主上 `java -jar … &` 启动。所以 java_parity 建议的「在 deploy/compose 的服务上用 ulimits: nofile」没有可落的对象。
- [confirmed] compat：Deployment 2 副本时 maxSurge 1 / maxUnavailable 0；gate STS OnDelete 一次排空一台；battle hostPort maxSurge 0 / maxUnavailable 1
   New-NodeDeploymentYaml（k8s_deploy.ps1:3540-3560）没有写 strategy，按 K8s 缺省 25%，2 副本时为 1/0（:1458 注释同口径）；k8s_client_entry.ps1:890-891 OnDelete；k8s_gate_drain.ps1:84-86、:1062 删 Pod 后等就绪；k8s_client_entry.ps1:1223-1227 为 0/1。均亲自打开。
- [unverifiable] compat：Agones Fleet 上 GameServer 退出后变 Unhealthy 并被替换，最多按 RollingUpdate 缺省 25% 少容量
   k8s_deploy.ps1:4058-4059 与 k8s_client_entry.ps1:1117-1118 只写了 `type: RollingUpdate`，没有 maxSurge/maxUnavailable；仓内找不到钉住的 Agones 版本。「Ready 之前崩溃」是原地 CrashLoop 还是转 Unhealthy 并重建，取决于 Agones 版本，未核实。
- [unverifiable] verify 第 3 步预计拒启退出码 134
   node.cpp:249-250 的 HandleFatalSignal 会恢复 SIG_DFL 再 raise。docker run 直接跑 ./gate 时 gate 是 PID 1，命名空间 init 会忽略非强制的缺省处置信号，所以退出码很可能不是 134（更可能是 139 或其它）。只断言「非 0」是对的，写「预计 134」会误导。
- [refuted] 工作区状态：README 未提交 +72/-23；PROGRESS +252（04:04）；PARITY +14/-2（03:32）
   我在核实时查到的数：deploy/k8s/README.md 是 `72 changed, 54 insertions(+), 18 deletions(-)`，调研者把总数 72 误读成新增行数；docs/PROGRESS.md 已到 +309，mtime 04:19:32；Java PARITY.md 已到 +15/-2，mtime 04:12:52。另外 k8s_deploy.ps1（04:10）和 deploy/k8s/AGENTS.md（04:11）也在被别的会话修改。node.cpp、core.vcxproj、filters、两份设计文档、SECURITY.md、runbook 仍然干净，与规格一致。
## plan_problems
- [should] 漏了裸 Linux 的启动路径。tools/scripts/deploy-staging.sh:132-146 直接后台启动 bin/scene 和 bin/gate，不设 GATE_RUN_MODE，也不等端口；配套手册是 docs/ops/linux-staging-stress-runbook.md:20-28。新的 fail-closed 门槛生效后，staging 主机的 shell 硬限如果低于 24096（例如没装 limits.d，或某些发行版缺省硬限 4096），gate 会立刻退出，脚本却照样打印 all services up（:160），FATAL 只落在 gate.log 里。以前这条路径能起（软限 1024 跑小规模压测），改动后会静默失败。
   修法: 在 plan 里补两处：①deploy-staging.sh 在 `sleep 5` 之后用 `kill -0 "$(cat ${PID_DIR}/gate.pid)"` 检查进程还活着，并 grep gate.log/scene.log 里的 `FATAL open file limit` 行，命中就 die 并打印该行（文件当前干净，mtime 10-12）；②linux-staging-stress-runbook.md 的 ulimit 一行补一句：进程会把软限自抬到硬限，硬限低于 GateMaxConnections+4096 时拒启。要是不想改脚本，至少在 risks 和 PROGRESS 里登记这条路径。
- [should] 新测试工程没有登记到 C++ 测试统一入口 tools/scripts/run_cpp_tests.ps1:45-73 的 $projects 表里。脚本头注释（:6-8）明确说不在表里的工程会悄悄烂掉；verify_commands 也只手工跑 exe，用户和 Codex 日常执行 `run_cpp_tests.ps1 -Build` 时不会覆盖到它。
   修法: 在 $projects 里按字母序追加 `'open_file_limit_test' = 'cpp\tests\open_file_limit_test\open_file_limit_test.vcxproj'`（文件当前干净，mtime 10-15 08:41）。verify 第 1 步改成 `pwsh tools/scripts/run_cpp_tests.ps1 -Build -Filter open_file_limit`，回归部分加 `-Filter client_endpoint` 等。
- [should] java_parity 的部署建议建立在错误事实上。Java 的 compose（deploy/compose/infra.yaml:11、:22-24）只有 MySQL/Redis/Kafka，game-gate/scene/battle 由 tools/local/start-slice.sh:284 在宿主上启动，「给 compose 服务加 ulimits: nofile」没有落点。规格还打算把这条建议写进 Java PARITY 行的备注，会把错误指引带到另一个仓。
   修法: 改写 java_parity 和 PARITY 备注：Java 游戏进程在宿主上起，生效的 nofile 取决于运行 start-slice.sh 的 shell 及其 systemd/limits.d（JVM 的 MaxFDLimit 自抬属外部常识，未核实）；Docker 镜像（Dockerfile:63）将来部署到容器运行时，会碰到与 C++ 同样的运行时缺省值。补救写成在启动断言里打印并比较 UnixOperatingSystemMXBean.getMaxFileDescriptorCount()，并在 start-slice.sh 启动前打印一次 `ulimit -Sn/-Hn`；去掉 compose ulimits 那条。
- [should] 上线次序全靠人工约定，代码层没有闸门。E11（各节点池的硬限）尚未实测，open_decisions 也没有落实到人；而工作区会被别的会话整区快照提交，下一次镜像构建就会把拒启逻辑带出去。Deployment 会停在滚动中途，相对安全；但全新 zone-up 或 all-up 会让整个 zone 起不来；Agones Fleet 的表现无法核实（见 fact_checks）。scene 的门槛 4096 没有任何配置可调，硬限不够时唯一的应急手段是回滚镜像。
   修法: 至少做到以下两点之一，并写进 compat_and_rollout：①在 PROGRESS 和交接里把「kind 上完成 verify 第 4 步、E11 有记录」列为发布新镜像的硬前置，并写清紧急回退就是换回旧镜像 tag（给出 k8s_image / zone-up 的具体参数）；②或者加一个只做运维豁免的显式环境变量（例如 NODE_NOFILE_ENFORCE=warn，与 RUN_MODE / GM 无关，缺省 enforce，生效时 stdout 和日志各打一行 WARN），但需要权衡它和 fail-closed 的取舍，并经负责人确认。不要默默依赖「大家会先读硬限」。
- [should] verify 第 3 步的红绿判据不可靠。(e) 要求旧镜像「30 秒内不退出」：裸 docker run 没有 Redis/etcd/Kafka，旧进程可能因无关原因提前退出，导致误判。(b) 先轮询 30 秒等日志行，再 `docker exec` 读 /proc/1/limits，进程可能已经因连不上基础设施退出，exec 读不到。「预计 134」在 gate 作为 PID 1 时也不成立（HandleFatalSignal 会以 SIG_DFL 重新 raise，init 会忽略这次信号）。
   修法: 红的判据改为：旧镜像输出里没有 `[open_file_limit]` 行，且在进程存活时读到软限 1024。读限值改成用 `docker run ... sh -c './gate & p=$!; sleep 1; grep "Max open files" /proc/$p/limits; wait $p'` 一类写法，或直接以启动行里的 soft= 字段作为主证据、/proc 读数只作辅证。退出码只断言非 0，并记录实际值。
- [nit] tests 声称「新头文件落地之前这些用例编译不过（这就是红）」。编译失败不是行为上的红，AGENTS §11.4 要求在无法先写回归测试时说明原因并给出替代验证。
   修法: 在 tests 里写明：纯函数单测无法在修复前呈现行为红，替代证据是第 3 步旧镜像与新镜像的容器级对照。
- [nit] kNonClientReserve 的论证只写了 scene 的 4Z 网状连接。直连模式的 gate 也连全服 SM（白名单 gate/main.cpp:232-233，SM 不是 zone-scoped，node_util.cpp:115-131），约 2Z 条；摘要里「4Z 同样超过 1024」在 C_z=100k 时也不成立（4Z=800）。
   修法: 注释和摘要改为：gate 非客户端连接约 2Z + zone 内 scene 数 + login/match，scene 约 4Z + gate 数，都在 4096 以内；去掉「同样超过 1024」，或改成「上半区间超过 1024」。
- [nit] 硬限为 RLIM_INFINITY 的边角情况：内核在 setrlimit(RLIMIT_NOFILE) 时会检查新的 rlim_max ≤ fs.nr_open，所以「硬限原样带回」会得到 EPERM。实际运行中 NOFILE 的硬限设不成 infinity，但单测 RaiseTargetCappedAtCeiling 显式覆盖了 kUnlimited，和系统调用层的行为不一致。另外 fs.nr_open 若在设硬限之后被调低，也会出现同样的 EPERM。
   修法: 在 node.cpp 的回调注释里写明这一点；EPERM 时可以把 rlim_max 降到 target 重试一次（硬限只降不升，非特权也允许），或者明确不处理，并在 risks 里登记。
- [nit] 工作区状态描述不准，且已经过时：README 实际是 +54/-18（总改动 72 行）；PROGRESS 已到 +309（04:19）；PARITY 已到 +15/-2（04:12）。k8s_deploy.ps1（04:10）和 deploy/k8s/AGENTS.md（04:11）也在被改。
   修法: 落码前按规格要求重新执行 git status / git diff --stat；README 只在 :1415 那一节和 :1467「## Kafka:StatefulSet + PVC」之间插入新节，不碰别人改的 MySQL/Redis 两节。
- [nit] P0-E 有部署后核对（README:996 的 `[RedisMaxClients]Mismatch`），本项只有启动行，没有任何自动核对。zone-up 失败时，运维要自己去翻 stderr。
   修法: 可选的后续项：在 k8s_deploy 的 -WaitReady 失败诊断里 grep `FATAL open file limit`，或者在就绪后抽查 `[open_file_limit]` 行。k8s_deploy.ps1 目前正被别的会话修改，本批不做，只在 risks 里登记。
## missed
- tools/scripts/deploy-staging.sh:132-146：裸 Linux staging 直接启动 bin/gate、bin/scene，不检查存活，拒启后会被静默吞掉（文件干净，mtime 2026-10-12 03:53）
- docs/ops/linux-staging-stress-runbook.md:20-28：staging 主机的 ulimit 要求要补「进程自抬软限、硬限不足拒启」（干净，mtime 10-15 04:34）
- tools/scripts/run_cpp_tests.ps1:45-73：C++ 测试统一入口的 $projects 表需登记 open_file_limit_test（干净，mtime 10-15 08:41）
- docs/design/gate-connection-admission-control.md:212 原文建议「参照 Redis 下调 maxclients」，规格偏离为直接拒启，落码说明里需要写明这次偏离和理由（AGENTS §11 偏离须记录）
- deploy/k8s/AGENTS.md（别的会话 04:11 正在改）：可以考虑加一条约束「C++ 启动壳不要再加 ulimit，fd 由进程自抬」，防止以后做出第二份。可选，冲突风险中
- Java 侧真实的启动路径 tools/local/start-slice.sh（宿主进程）应写进 java_parity，替代错误的 compose 建议
- 回退手段（换回旧镜像 tag 的具体命令）没有写进 compat_and_rollout，scene 也没有可调的配置键作为应急手段
