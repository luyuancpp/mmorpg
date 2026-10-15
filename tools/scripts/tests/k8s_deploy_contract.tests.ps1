#requires -Version 7
<#
.SYNOPSIS
    k8s_deploy.ps1 的部署生成器契约测试。

.DESCRIPTION
    背景:k8s_deploy.ps1 1600+ 行、零测试,产物又不入库(每次 apply 现生成),
    于是它写死的常数和各服务 etc/*.yaml 之间静默漂移过一堆压测期已知会炸的值:
      - Locker.PlayerLockTTL       生成器 5   vs etc 120(30s 都被证明会丢锁)
      - Kafka.PartitionCnt         生成器 5   vs etc 10
      - Database.MaxOpenConn       生成器 10  vs etc 60(10 就是那个瓶颈)
      - Database.User/Passwd       生成器 root/root vs 集群 MYSQL_ROOT_PASSWORD

    所以本套测试的核心判据是**跨文件一致**:生成的 ConfigMap 值必须等于服务
    自己 etc/*.yaml 里的值,而不是"生成器里也有一份差不多的常数"。

    负向用例一律断言**错误文本**,不只断退出码 —— 退出码 1 可能来自任何地方
    (打错参数、脚本语法错),只断退出码的负向测试等于没测。

.EXAMPLE
    pwsh -File tools/scripts/tests/k8s_deploy_contract.tests.ps1
#>

$ErrorActionPreference = "Stop"

. "$PSScriptRoot/lib/test_harness.ps1"
. "$PSScriptRoot/lib/deploy_capture.ps1"

Write-Host ""
Write-Host "=== k8s_deploy.ps1 deployment generator contract tests ==="

# 一次 DryRun 供多个用例复用(子进程启动比断言贵得多)
$BaseArgs = @(
    "-Command", "zone-up",
    "-ZoneName", "contract-test",
    "-ZoneId", "101",
    "-DryRun",
    "-GoSvcRegistry", "registry.invalid/test",
    "-JavaSvcRegistry", "registry.invalid/test"
)

$devRun = Invoke-DeployDryRun -Arguments $BaseArgs
if ($devRun.ExitCode -ne 0) {
    Write-Host $devRun.Output
    throw "基线 DryRun 自己就失败了(exit=$($devRun.ExitCode)),后续断言无意义"
}
$devOut = $devRun.Output

# Agones 渲染路径复用同一次 DryRun(Fleet 与 Deployment 是两套模板,sidecar / 日志保留
# 这类断言必须两边都过 —— 只改一边正是本仓踩过的坑)。
$agonesRun = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-SceneOrchestrator', 'agones', '-SkipGoSvc', '-SkipJavaSvc'))
if ($agonesRun.ExitCode -ne 0) {
    Write-Host $agonesRun.Output
    throw "Agones DryRun 基线自己就失败了(exit=$($agonesRun.ExitCode)),后续断言无意义"
}
$agonesOut = $agonesRun.Output

# ─────────────────────────────────────────────────────────────────
# 0. 采集链路自检
# ─────────────────────────────────────────────────────────────────

Test-Case "自检:子进程输出的中文必须无损带回(否则后面断错误文本的用例全是假红)" {
    # 本套测试的负向用例断的是子进程打出来的中文错误文本。PowerShell 按
    # [Console]::OutputEncoding 解码原生进程输出,而 Windows 控制台默认码页是
    # ibm437/GBK 这类 OEM 码页 —— 中文会在编解码往返里被整体打成 '?',于是 6 条
    # 断中文的用例集体变红,红的却是编码链路而不是被测脚本。这个坑真踩过一次,
    # 排查成本远高于这条自检本身,所以在这里钉一条。
    # 兜底逻辑在 tests/lib/deploy_capture.ps1 的 Invoke-CapturedPwsh。
    #
    # 判据用"输出里还有没有汉字",不锚定具体措辞:生成器往 ConfigMap 里写的
    # 中文注释会随代码改,而"一个汉字都不剩"只可能是编码坏了。
    # 注意本条只能抓住"汉字被打成 '?'"这一类(ibm437 路径);GBK 误解码可能
    # 仍落在汉字区间,那种情况由下面各条断具体错误文本的用例兜住。
    Assert-Match -Text $devOut -Pattern '[一-龥]' -Because "捕获回来的子进程输出里一个汉字都没有 = 采集链路的编码坏了,不是被测脚本的问题"
}

# ─────────────────────────────────────────────────────────────────
# 1. 生成的 ConfigMap 必须与各服务 etc/*.yaml 跨文件一致
# ─────────────────────────────────────────────────────────────────

Test-Case "db ConfigMap 的 Kafka/Database 关键值 == go/db/etc/db.yaml" {
    $block = Select-ManifestByName -Output $devOut -Name "go-svc-db-config"
    Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 go-svc-db-config"
    $flat = ConvertTo-FlatManifest -Block $block

    $pairs = @(
        @{ Gen = 'data.db.yaml.ServerConfig.Kafka.PartitionCnt';    Etc = 'ServerConfig.Kafka.PartitionCnt' }
        @{ Gen = 'data.db.yaml.ServerConfig.Kafka.TopicGeneration'; Etc = 'ServerConfig.Kafka.TopicGeneration' }
        @{ Gen = 'data.db.yaml.ServerConfig.Kafka.SubShardCount';   Etc = 'ServerConfig.Kafka.SubShardCount' }
        @{ Gen = 'data.db.yaml.ServerConfig.Database.MaxOpenConn';  Etc = 'ServerConfig.Database.MaxOpenConn' }
        @{ Gen = 'data.db.yaml.ServerConfig.Database.MaxIdleConn';  Etc = 'ServerConfig.Database.MaxIdleConn' }
    )
    foreach ($p in $pairs) {
        $expected = Get-EtcValue -RelativePath 'go/db/etc/db.yaml' -KeyPath $p.Etc
        $actual = Get-FlatValue -Flat $flat -KeyPath $p.Gen
        Assert-Equal -Expected $expected -Actual $actual -Because "$($p.Etc) 必须与 go/db/etc/db.yaml 一致(生成器不得自带常数)"
    }
}

Test-Case "data-service ConfigMap 的 Kafka 关键值 == go/data_service/etc/data_service.yaml" {
    # 审计 topic 的世代 / 分区数是不可变契约,player_snapshot 消费者的批量 / 并发旋钮与 lag 探针(G2a)决定落库路径:
    # 生成器一旦自带常数,集群跑的就不是仓库里评审过的那份(比如服务 yaml 仍是逐条,ConfigMap 却悄悄开了批量)。
    $block = Select-ManifestByName -Output $devOut -Name "go-svc-data-service-config"
    Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 go-svc-data-service-config"
    $flat = ConvertTo-FlatManifest -Block $block

    $keys = @(
        'TopicGeneration', 'TransactionLogPartitions', 'SnapshotPartitions', 'RetentionMs',
        'SnapshotBatchMaxRows', 'SnapshotBatchMaxBytes', 'SnapshotFlushIntervalMs', 'SnapshotMaxInflightFlushes', 'LagProbeIntervalMs'
    )
    foreach ($k in $keys) {
        $expected = Get-EtcValue -RelativePath 'go/data_service/etc/data_service.yaml' -KeyPath "Kafka.$k"
        $actual = Get-FlatValue -Flat $flat -KeyPath "data.data_service.yaml.Kafka.$k"
        Assert-Equal -Expected $expected -Actual $actual -Because "Kafka.$k 必须与 go/data_service/etc/data_service.yaml 一致(生成器不得自带常数)"
    }
}

Test-Case "ConfigMap 镜像 MySQL 池参数 == 服务 yaml(db Placement、data-service PlayerName / RouteStore、gateway Hikari),login 不写 PasswordAuth" {
    # 背景(G2b,docs/handoff/launch-readiness-20261013/spec_G2b.md §4.1.4):所有 zone 的 go/db、data_service、Java gateway 共用同一台
    # MySQL,连接总账(k8s_deploy.ps1 的 Get-MysqlConnectionLedgerInputs)读的是服务自己的配置。这几组池参数以前不进 ConfigMap,
    # 集群里生效的是「Go 默认值 / jar 内那份恰好等于服务 yaml」:两边一漂移,总账算的就不是集群的账,而且不会有任何报错。
    # db 的额外落点库池按库所属家族分两个旋钮(ZoneStore* / NonZoneStore*),外加池内空闲连接的回收秒数,五个键都镜像。
    # 字段名用 PoolKeys 而不是 Keys:哈希表自带 Keys 属性,同名字段读起来有歧义。
    $mirrors = @(
        @{ ConfigMap = 'go-svc-db-config'; Prefix = 'data.db.yaml.'; Source = 'go/db/etc/db.yaml'
           PoolKeys = @('Placement.ZoneStoreMaxOpenConn', 'Placement.ZoneStoreMaxIdleConn', 'Placement.NonZoneStoreMaxOpenConn', 'Placement.NonZoneStoreMaxIdleConn', 'Placement.ExtraStoreConnMaxIdleSeconds') }
        @{ ConfigMap = 'go-svc-data-service-config'; Prefix = 'data.data_service.yaml.'; Source = 'go/data_service/etc/data_service.yaml'
           PoolKeys = @('PlayerName.MaxOpenConn', 'PlayerName.MaxIdleConn', 'RouteStore.MaxOpenConn', 'RouteStore.MaxIdleConn') }
        @{ ConfigMap = 'java-svc-gateway-config'; Prefix = 'data.application.yaml.'; Source = 'java/gateway_node/src/main/resources/application.yaml'
           PoolKeys = @('spring.datasource.hikari.maximum-pool-size', 'spring.datasource.hikari.minimum-idle') }
    )
    foreach ($mirror in $mirrors) {
        $block = Select-ManifestByName -Output $devOut -Name $mirror.ConfigMap
        Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 $($mirror.ConfigMap)"
        $flat = ConvertTo-FlatManifest -Block $block
        foreach ($key in $mirror.PoolKeys) {
            $expected = Get-EtcValue -RelativePath $mirror.Source -KeyPath $key
            $actual = Get-FlatValue -Flat $flat -KeyPath ($mirror.Prefix + $key)
            Assert-Equal -Expected $expected -Actual $actual -Because "$($mirror.ConfigMap) 的 $key 必须与 $($mirror.Source) 一致(生成器不得自带常数;MySQL 连接总账读的是同一个键)"
        }
    }

    # login 的 PasswordAuth 自带一个 MySQL 连接池(go/login/etc/login.yaml 里 MaxOpenConns 20),总账没有登记它:前提是 K8s 的
    # login ConfigMap 不写这一段、默认不启用。一旦写出来,每个 login 实例就多一组不在账上的连接 —— 要启用,先在
    # Get-MysqlConnectionLedgerInputs / Get-MysqlConnectionLedger 里登记,再改这里。
    # DevPasswordAuth 是另一个顶层键(拍平后是 data.login.yaml.DevPasswordAuth...),不会被这条判据误中。
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'go-svc-login-config')
    $passwordAuthKeys = @($loginFlat.Scalars.Keys | Where-Object { $_ -eq 'data.login.yaml.PasswordAuth' -or $_ -like 'data.login.yaml.PasswordAuth.*' })
    Assert-True -Condition ($passwordAuthKeys.Count -eq 0) -Because "login ConfigMap 不得写 PasswordAuth 段(实际出现:$($passwordAuthKeys -join ', ')):它的 MySQL 连接池不在连接总账里"
}

Test-Case "login ConfigMap 的 Timeout/Node/Locker/Kafka 关键值 == go/login/etc/login.yaml" {
    $block = Select-ManifestByName -Output $devOut -Name "go-svc-login-config"
    Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 go-svc-login-config"
    $flat = ConvertTo-FlatManifest -Block $block

    # Timeout:C++ deadline 预算门禁(Assert-GrpcClientDeadlineBudget)核对的是 login.yaml,ConfigMap 必须是同一个值,
    # 否则门禁放行的不是集群里真正生效的那份(生成器以前在模板里写死 100000)。
    $pairs = @(
        @{ Gen = 'data.login.yaml.Timeout';                Etc = 'Timeout' }
        @{ Gen = 'data.login.yaml.Node.SessionExpireMin';  Etc = 'Node.SessionExpireMin' }
        @{ Gen = 'data.login.yaml.Node.MaxLoginDevices';   Etc = 'Node.MaxLoginDevices' }
        @{ Gen = 'data.login.yaml.Node.LeaseTTL';          Etc = 'Node.LeaseTTL' }
        @{ Gen = 'data.login.yaml.Node.QueueShardCount';   Etc = 'Node.QueueShardCount' }
        @{ Gen = 'data.login.yaml.Locker.AccountLockTTL';  Etc = 'Locker.AccountLockTTL' }
        @{ Gen = 'data.login.yaml.Locker.PlayerLockTTL';   Etc = 'Locker.PlayerLockTTL' }
        @{ Gen = 'data.login.yaml.Kafka.PartitionCnt';     Etc = 'Kafka.PartitionCnt' }
        @{ Gen = 'data.login.yaml.Kafka.InitialPartition'; Etc = 'Kafka.InitialPartition' }
        @{ Gen = 'data.login.yaml.Kafka.TopicGeneration';  Etc = 'Kafka.TopicGeneration' }
    )
    foreach ($p in $pairs) {
        $expected = Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath $p.Etc
        $actual = Get-FlatValue -Flat $flat -KeyPath $p.Gen
        Assert-Equal -Expected $expected -Actual $actual -Because "$($p.Etc) 必须与 go/login/etc/login.yaml 一致"
    }
}

Test-Case "player-locator ConfigMap 的 LeaseTTL == go/player_locator/etc/player_locator.yaml" {
    $block = Select-ManifestByName -Output $devOut -Name "go-svc-player-locator-config"
    Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 go-svc-player-locator-config"
    $flat = ConvertTo-FlatManifest -Block $block

    foreach ($key in @('Node.LeaseTTL', 'Lease.DefaultTTLSeconds')) {
        $expected = Get-EtcValue -RelativePath 'go/player_locator/etc/player_locator.yaml' -KeyPath $key
        $actual = Get-FlatValue -Flat $flat -KeyPath "data.player_locator.yaml.$key"
        Assert-Equal -Expected $expected -Actual $actual -Because "$key 必须与 player_locator.yaml 一致"
    }
}

Test-Case "zone 内 go-svc Deployment 统一注入控制面命令 topic 契约,值 == bin/etc/base_deploy_config.yaml" {
    # 背景:go/shared/kafkacmd 不配 KAFKA_COMMAND_TOPIC_* 就回落到 256 / 1,而 C++ gate / scene 与 kafka-topic-init
    # 用的是 base_deploy_config.yaml 的代号(当前 2)。两边不一致时 login 的会话绑定、顶号踢人、scene-manager 的换场景
    # 等 Go → gate / scene 命令落进没人消费的 topic,静默丢失;本机 start_game.ps1 会注入,所以只有 K8s 上才出事。
    $partitions = Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'Kafka.CommandTopicPartitions'
    $generation = Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'Kafka.CommandTopicGeneration'
    foreach ($name in @('login', 'player-locator', 'scene-manager', 'db', 'data-service')) {
        # 按「kind: Deployment + metadata 里的名字」挑块:ConfigMap 里 go-zero 的 Name: 字段也可能等于服务名。
        $block = @(Get-ManifestBlocks -Output $devOut | Where-Object {
            $_ -cmatch '(?m)^kind: Deployment\s*$' -and $_ -cmatch "(?m)^  name: $([regex]::Escape($name))\s*$"
        }) | Select-Object -First 1
        Assert-True -Condition ($null -ne $block) -Because "DryRun 输出里应当有 $name 的 Deployment"
        Assert-Match -Text $block -Pattern "- name: KAFKA_COMMAND_TOPIC_PARTITIONS\s+value: `"$partitions`"" -Because "$name 的命令分区数必须与 C++ / topic 预建同一契约"
        Assert-Match -Text $block -Pattern "- name: KAFKA_COMMAND_TOPIC_GENERATION\s+value: `"$generation`"" -Because "$name 的命令代号必须与 C++ / topic 预建同一契约"
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, 'name: KAFKA_COMMAND_TOPIC_GENERATION').Count) -Because '只能注入一份'
        # topic 副本数同样统一注入:dev 档是单 broker,所以是 1(多 broker 的取值见下面「Kafka 拓扑」一节)。
        Assert-Match -Text $block -Pattern '- name: KAFKA_TOPIC_REPLICATION_FACTOR\s+value: "1"' -Because "$name 建 topic 的副本数必须与 broker 数同源,不能各服务各写各的"
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, 'name: KAFKA_TOPIC_REPLICATION_FACTOR').Count) -Because '只能注入一份'
    }
}

Test-Case "data-service 是多副本形态:2 副本滚动更新 + PDB + 反亲和,ConfigMap 要求 store 必备,迁移 Job 先于 Deployment" {
    # 背景(docs/design/no-single-node-horizontal-scaling-20261001.md §4):data-service 管发号段与审计落库,
    # 以前钉成 1 副本 + Recreate。多副本的两个服务侧前提是迁移互斥与 Store.Required,部署侧要一起到位。
    $block = @(Get-ManifestBlocks -Output $devOut | Where-Object {
        $_ -cmatch '(?m)^kind: Deployment\s*$' -and $_ -cmatch '(?m)^  name: data-service\s*$'
    }) | Select-Object -First 1
    Assert-True -Condition ($null -ne $block) -Because 'DryRun 输出里应当有 data-service 的 Deployment'
    Assert-Match -Text $block -Pattern '(?m)^  replicas: 2\s*$' -Because '单副本 = 这个 zone 的发号与审计落库是单点'
    Assert-NotMatch -Text $block -Pattern 'type: Recreate' -Because 'Recreate 会在发布时出现一个实例都没有的窗口'
    Assert-Match -Text $block -Pattern 'type: RollingUpdate\s+rollingUpdate:\s+maxSurge: 1\s+maxUnavailable: 0' -Because '先起新的再停旧的'
    Assert-Match -Text $block -Pattern 'podAntiAffinity' -Because '两个副本不该落在同一个节点上'
    Assert-Match -Text $block -Pattern '(?s)kind: PodDisruptionBudget.*?name: data-service-pdb.*?minAvailable: 1' -Because 'PDB 必须写在主 manifest 里,独立文件不会被 apply'
    Assert-Match -Text $block -Pattern 'fieldPath: status.podIP' -Because 'C++ 约定注册仍要通告 Pod IP'
    Assert-Match -Text $block -Pattern 'containerPort: 9260\s+name: metrics' -Because '多副本后要能按 Pod 抓指标'

    $configMap = Select-ManifestByName -Output $devOut -Name 'go-svc-data-service-config'
    Assert-Match -Text $configMap -Pattern '(?m)^\s+Store:\s*\r?\n\s+Required: true\s*$' -Because 'store 装配不起来的实例不能进发现池'
    Assert-Match -Text $configMap -Pattern 'MetricsListenAddr: ":9260"' -Because '指标端口要与 manifest 一致'

    $job = Select-ManifestByName -Output $devOut -Name 'data-service-migrate'
    Assert-True -Condition ($null -ne $job) -Because 'staging/prod 启动期不建表、不种号段行,必须有迁移 Job'
    Assert-Match -Text $job -Pattern 'args: \["-f", "/app/etc/data_service.yaml", "-migrate"\]' -Because 'Job 必须用与服务同一份配置跑 -migrate'
    # (?-i):占位是全大写;断言助手默认不分大小写,不关掉会把无关的小写双下划线词也算进去。
    Assert-NotMatch -Text $job -Pattern '(?-i)__[A-Z][A-Z0-9_]*__|PLACEHOLDER_' -Because '占位必须全部替换'
    $jobIndex = $devOut.IndexOf('name: data-service-migrate')
    $deployIndex = $devOut.IndexOf('name: data-service-pdb')
    Assert-True -Condition ($jobIndex -ge 0 -and $deployIndex -gt $jobIndex) -Because '迁移 Job 必须先于 Deployment apply'
}


Test-Case "db 是多副本形态:2 副本滚动更新 + PDB + 反亲和" {
    # 背景(docs/design/no-single-node-horizontal-scaling-20261001.md §3):db 把存档任务从 Kafka 落到 MySQL,以前每个 zone 钉成单实例。
    # 重试收据按实例归属之后(go/db/internal/kafka/retry_ownership.go)可以多开;部署侧要一起到位。
    $block = @(Get-ManifestBlocks -Output $devOut | Where-Object {
        $_ -cmatch '(?m)^kind: Deployment\s*$' -and $_ -cmatch '(?m)^  name: db\s*$'
    }) | Select-Object -First 1
    Assert-True -Condition ($null -ne $block) -Because 'DryRun 输出里应当有 db 的 Deployment'
    Assert-Match -Text $block -Pattern '(?m)^  replicas: 2\s*$' -Because '单副本 = 这个 zone 的存档落库是单点'
    Assert-Match -Text $block -Pattern 'type: RollingUpdate\s+rollingUpdate:\s+maxSurge: 1\s+maxUnavailable: 0' -Because '先起新的再停旧的,发布期间不出现零实例'
    Assert-Match -Text $block -Pattern 'podAntiAffinity' -Because '两个副本不该落在同一个节点上'
    Assert-Match -Text $block -Pattern '(?s)kind: PodDisruptionBudget.*?name: db-pdb.*?minAvailable: 1' -Because 'PDB 必须写在主 manifest 里,独立文件不会被 apply'
    Assert-Match -Text $block -Pattern 'terminationGracePeriodSeconds: 45' -Because '停机要留出把重试收据还回 ready 的时间'
}

Test-Case 'db 指标端口:ConfigMap MetricsListenAddr ":9160" 与 manifest 9160 metrics 端口 + prometheus 注解一致' {
    # 背景(G2b):以前 K8s 上 db 的 ConfigMap 不写 MetricsListenAddr(缺省空串 = go/db 不起 /metrics 端口),manifest 也没有这个端口,
    # 连接池指标 mmorpg_sql_pool_*{sql_client="db"} 与 deploy/k8s/owner-epoch-alerts.yaml 的抓取前提在集群里都不成立。
    # 三处要一起对上:ConfigMap 让进程监听,容器端口声明它,注解让 Prometheus 来抓。只对上一部分时没有任何报错 ——
    # 没有序列的告警规则不触发,和「一切正常」长得一样(deploy/k8s/mysql-connection-alerts.yaml 文件头的抓取前提)。
    $block = @(Get-ManifestBlocks -Output $devOut | Where-Object {
        $_ -cmatch '(?m)^kind: Deployment\s*$' -and $_ -cmatch '(?m)^  name: db\s*$'
    }) | Select-Object -First 1
    Assert-True -Condition ($null -ne $block) -Because 'DryRun 输出里应当有 db 的 Deployment'
    # 按行首锚定:manifest 的注释里也提到 9160 与 prometheus.io/port,不能让注释把断言喂饱。
    Assert-Match -Text $block -Pattern '(?m)^[ ]+- containerPort: 9160\s+name: metrics\s*$' -Because '容器必须声明名为 metrics 的 9160 端口(与 data-service 的 9260 同形)'
    Assert-Match -Text $block -Pattern '(?m)^[ ]+prometheus\.io/scrape: "true"\s*$' -Because 'Pod 模板必须带抓取注解,否则指标端口开了也没人抓'
    Assert-Match -Text $block -Pattern '(?m)^[ ]+prometheus\.io/port: "9160"\s*$' -Because '抓取端口注解必须与容器的 metrics 端口一致'
    Assert-Match -Text $block -Pattern '(?m)^[ ]+prometheus\.io/path: "/metrics"\s*$' -Because '抓取路径注解必须是 /metrics'

    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'go-svc-db-config')
    Assert-Equal -Expected ':9160' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.db.yaml.MetricsListenAddr') -Because 'ConfigMap 不写(或写别的端口)时,manifest 声明的 9160 上没有监听,抓取恒失败'
}


Test-Case "login 与 db 生成产物里的 Kafka.PartitionCnt 必须彼此相等" {
    # db 侧启动门禁 fail-closed:两边不一致直接起不来
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-login-config")
    $dbFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-db-config")
    $loginVal = Get-FlatValue -Flat $loginFlat -KeyPath 'data.login.yaml.Kafka.PartitionCnt'
    $dbVal = Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.ServerConfig.Kafka.PartitionCnt'
    Assert-Equal -Expected $loginVal -Actual $dbVal -Because "login/db 的 PartitionCnt 不一致时 db 启动门禁会 fail-closed"
}

Test-Case "scene-manager ConfigMap 必须带 GateTokenSecret(否则跨 zone 重定向签不了令牌)" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-scene-manager-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.scene_manager_service.yaml.GateTokenSecret'
    Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($v)) -Because "gate_redirect.go 在空值时直接返回 'GateTokenSecret not configured'"
}

Test-Case "scene-manager ConfigMap 的 Timeout / KafkaWriteTimeoutSeconds / HomeZoneLookupTimeoutMs == go/scene_manager/etc/scene_manager_service.yaml" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-scene-manager-config")
    foreach ($key in @('Timeout', 'KafkaWriteTimeoutSeconds', 'HomeZoneLookupTimeoutMs')) {
        $expected = Get-EtcValue -RelativePath 'go/scene_manager/etc/scene_manager_service.yaml' -KeyPath $key
        $actual = Get-FlatValue -Flat $flat -KeyPath "data.scene_manager_service.yaml.$key"
        Assert-Equal -Expected $expected -Actual $actual -Because "$key 必须与 scene_manager_service.yaml 一致(生成器不得自带常数)"
    }
}

# EnterScene 第一条腿在"归属查询 + Kafka 同步写"之外的 Redis 往返余量(毫秒):读 location、预占频道、铸造 / CAS Lua、
# 路由失败时的回滚 Lua。口径 = cross-zone-scene-travel.md §12.2,与 scene_manager_service.yaml 的 Timeout 注释
# (8000 = 1500 + 5000 + 1500)是同一个数;全仓只在这里做判定。
$SceneManagerEnterSceneMarginMs = 1500

# scene-manager 超时预算判定。$Scalars = ConvertFrom-YamlToFlatMap 的 Scalars;$KeyPrefix 对 ConfigMap 是
# 'data.scene_manager_service.yaml.',对服务 yaml 是 ''。返回违例文本数组,空 = 通过。
function Get-SceneManagerTimeoutBudgetViolations {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Scalars,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$KeyPrefix
    )
    $violations = New-Object System.Collections.Generic.List[string]

    # go-zero(zrpc/server.go)只在 Timeout > 0 时装超时拦截器并带上 MethodTimeouts;k8s_deploy.ps1 只镜像标量 Timeout、
    # 不搬 MethodTimeouts。用它只放宽 EnterScene 时本地生效、K8s 静默退回全局值 —— 真要用,先在生成器里整块镜像
    # (Get-AuthoritativeYamlBlock)再改这里。
    $methodKeys = @($Scalars.Keys | Where-Object { $_ -like "${KeyPrefix}MethodTimeouts*" })
    if ($methodKeys.Count -gt 0) {
        $violations.Add("出现 MethodTimeouts($($methodKeys -join ', ')):k8s_deploy.ps1 不镜像它,只许用全局 Timeout")
    }

    # 三项都要显式正整数:Timeout <= 0 = 不装超时拦截器,EnterScene 应答时刻无上界;另两项 <= 0 时运行期回落默认值
    # (svc/servicecontext.go、logic/home_zone.go),这里不另抄一份默认值去猜。
    $values = @{}
    foreach ($key in @('Timeout', 'KafkaWriteTimeoutSeconds', 'HomeZoneLookupTimeoutMs')) {
        $raw = $Scalars["$KeyPrefix$key"]
        $parsed = [long]0
        if ($null -eq $raw -or -not [long]::TryParse([string]$raw, [ref]$parsed) -or $parsed -le 0) {
            $violations.Add("$key='$raw' 必须是正整数")
            continue
        }
        $values[$key] = $parsed
    }
    if ($values.Count -eq 3) {
        $need = $values['HomeZoneLookupTimeoutMs'] + $values['KafkaWriteTimeoutSeconds'] * 1000 + $SceneManagerEnterSceneMarginMs
        if ($values['Timeout'] -lt $need) {
            $violations.Add(("Timeout={0} < HomeZoneLookupTimeoutMs({1}) + KafkaWriteTimeoutSeconds({2})*1000 + {3} = {4}" -f $values['Timeout'], $values['HomeZoneLookupTimeoutMs'], $values['KafkaWriteTimeoutSeconds'], $SceneManagerEnterSceneMarginMs, $need))
        }
    }
    return $violations.ToArray()
}

Test-Case "scene-manager 的 zrpc Timeout 必须为正、盖住归属查询 + Kafka 同步写 + 1500ms 余量且不用 MethodTimeouts(否则 EnterScene 的失败应答回不到 C++ 源 scene)" {
    # 服务 yaml 与 K8s 产物各算一遍:以后谁调大 KafkaWriteTimeoutSeconds / HomeZoneLookupTimeoutMs 而忘了同步调
    # Timeout,或改用 MethodTimeouts,这里点名失败(cross-zone-scene-travel.md §12.2)。
    $yamlPath = 'go/scene_manager/etc/scene_manager_service.yaml'
    $views = @(
        @{ Name = 'K8s ConfigMap go-svc-scene-manager-config'; Prefix = 'data.scene_manager_service.yaml.'
           Scalars = (ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-scene-manager-config")).Scalars }
        @{ Name = $yamlPath; Prefix = ''
           Scalars = (ConvertFrom-YamlToFlatMap -Text (Get-Content -LiteralPath (Join-Path (Get-RepoRoot) $yamlPath) -Raw)).Scalars }
    )
    foreach ($v in $views) {
        $violations = @(Get-SceneManagerTimeoutBudgetViolations -Scalars $v.Scalars -KeyPrefix $v.Prefix)
        Assert-True -Condition ($violations.Count -eq 0) -Because ("{0}:{1}" -f $v.Name, ($violations -join ';'))
    }
}

Test-Case "自检:scene-manager 超时预算判定对 5000 / 2000 / 7999 / 0 / 缺键 / MethodTimeouts 报违例、对 8000 通过(守卫不得静默放行)" {
    $pass = (ConvertFrom-YamlToFlatMap -Text "Timeout: 8000`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500").Scalars
    $got = @(Get-SceneManagerTimeoutBudgetViolations -Scalars $pass -KeyPrefix '')
    Assert-True -Condition ($got.Count -eq 0) -Because ("8000 = 1500 + 5000 + 1500 应通过,实际报:{0}" -f ($got -join ';'))
    $cases = @(
        @{ Why = '改前本地值 5000'; Text = "Timeout: 5000`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = '改前 K8s 落的 go-zero 默认 2000'; Text = "Timeout: 2000`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = '差 1ms(余量是 1500 不是 1000)'; Text = "Timeout: 7999`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = 'Timeout 0 = 不装超时拦截器'; Text = "Timeout: 0`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = '缺 Timeout 键(go-zero 会落默认 2000)'; Text = "KafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = 'KafkaWriteTimeoutSeconds 0(运行期回落 5s,判定不猜)'; Text = "Timeout: 8000`nKafkaWriteTimeoutSeconds: 0`nHomeZoneLookupTimeoutMs: 1500" }
        @{ Why = 'MethodTimeouts 只放宽 EnterScene(K8s 不镜像)'; Text = "Timeout: 8000`nKafkaWriteTimeoutSeconds: 5`nHomeZoneLookupTimeoutMs: 1500`nMethodTimeouts:`n  - FullMethod: /scene_manager.SceneManager/EnterScene`n    Timeout: 8s" }
    )
    foreach ($c in $cases) {
        $got = @(Get-SceneManagerTimeoutBudgetViolations -Scalars (ConvertFrom-YamlToFlatMap -Text $c.Text).Scalars -KeyPrefix '')
        Assert-True -Condition ($got.Count -gt 0) -Because "$($c.Why) 必须报违例"
    }
    $prefixed = [ordered]@{ 'data.scene_manager_service.yaml.Timeout' = '7999'; 'data.scene_manager_service.yaml.KafkaWriteTimeoutSeconds' = '5'; 'data.scene_manager_service.yaml.HomeZoneLookupTimeoutMs' = '1500' }
    Assert-True -Condition (@(Get-SceneManagerTimeoutBudgetViolations -Scalars $prefixed -KeyPrefix 'data.scene_manager_service.yaml.').Count -gt 0) -Because "ConfigMap 前缀下的 7999 也必须报违例"
}

# ─────────────────────────────────────────────────────────────────
# C++ gRPC 客户端 deadline 预算(docs/design/grpc-client-deadline-failure-callback.md §4.2 / §4.4)
# ─────────────────────────────────────────────────────────────────
# 上面守的是 scene-manager 服务端 Timeout 盖住它自己的内部预算;这里守反方向:C++ deadline ≥ 目标 Go 服务的
# zrpc Timeout + 2000(上游比下游宽),以及 node ConfigMap 把 GrpcClient 块原样搬进集群。
# 判定规则只有一份,在 k8s_deploy.ps1:Get-GrpcClientDeadlineBudgetViolations(纯函数)+ Assert-GrpcClientDeadlineBudget
# (读文件、不满足即 throw,由写路径入口调用)。这里按 AST 把这组函数(名单见下)抽出来直接调用,不另抄一份判定,也不执行脚本入口
# (与 k8s_migrate_gate.tests.ps1 同一做法)。它们用到的 ConvertFrom-YamlToFlatMap 来自 deploy_capture.ps1 dot-source 的
# release_common.ps1;Assert 读 $RepoRoot,由各用例自己设。
$deadlineParseTokens = $null
$deadlineParseErrors = $null
$deadlineDeployAst = [System.Management.Automation.Language.Parser]::ParseFile((Join-Path (Get-ToolsScriptsDir) 'k8s_deploy.ps1'), [ref]$deadlineParseTokens, [ref]$deadlineParseErrors)
if ($deadlineParseErrors.Count -gt 0) { throw "k8s_deploy.ps1 语法错误: $($deadlineParseErrors.Message -join '; ')" }
$deadlineFunctionNames = @(
    'ConvertTo-ZrpcDurationMs', 'Get-ZrpcMethodTimeouts', 'Get-GrpcClientMethodsNotCalledFromCpp',
    'Get-GrpcClientDeadlineBudgetViolations', 'Assert-GrpcClientDeadlineBudget'
)
foreach ($deadlineFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($deadlineFunctionNames -contains $deadlineFnAst.Name) {
        Set-Item -Path "Function:script:$($deadlineFnAst.Name)" -Value $deadlineFnAst.Body.GetScriptBlock()
    }
}
foreach ($deadlineFnName in $deadlineFunctionNames) {
    if (-not (Test-Path "Function:$deadlineFnName")) { throw "k8s_deploy.ps1 缺少函数 $deadlineFnName(deadline 预算守卫被删了?后续断言无意义)" }
}

# ConfigMap 拍平后的键带 data.<文件名>. 前缀;去掉它才能与服务 yaml 用同一个判定函数、同一套键名。
function ConvertTo-UnprefixedScalars {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Scalars,
        [Parameter(Mandatory = $true)][string]$Prefix
    )
    $out = [ordered]@{}
    foreach ($key in $Scalars.Keys) {
        if (([string]$key).StartsWith($Prefix, [System.StringComparison]::Ordinal)) {
            $out[([string]$key).Substring($Prefix.Length)] = $Scalars[$key]
        }
    }
    return $out
}

# node ConfigMap 以 readOnly 整目录挂到 /app/bin/etc,完全遮蔽镜像里的 base_deploy_config.yaml:漏搬 GrpcClient 块 =
# K8s 上所有目标落回 C++ 内置默认 10000,号段 fetchTimeout / 换图在途 TTL 跟着偏离仓库口径,且不会有任何报错。
function Assert-GrpcClientBlockMirrored {
    param(
        [Parameter(Mandatory = $true)][string]$Output,
        [Parameter(Mandatory = $true)][string]$ConfigMapName
    )
    $authoritative = (ConvertFrom-YamlToFlatMap -Text (Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'bin/etc/base_deploy_config.yaml') -Raw)).Scalars
    $expectedKeys = @($authoritative.Keys | Where-Object { $_ -like 'GrpcClient.CallDeadlineMs.*' })
    Assert-True -Condition ($expectedKeys.Count -gt 0) -Because 'bin/etc/base_deploy_config.yaml 里应当有 GrpcClient.CallDeadlineMs(本断言的前提)'
    $generated = ConvertTo-UnprefixedScalars -Scalars (ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $Output -Name $ConfigMapName)).Scalars -Prefix 'data.base_deploy_config.yaml.'
    $generatedKeys = @($generated.Keys | Where-Object { $_ -like 'GrpcClient.CallDeadlineMs.*' })
    Assert-Equal -Expected ($expectedKeys -join ',') -Actual ($generatedKeys -join ',') -Because "$ConfigMapName 的 GrpcClient.CallDeadlineMs 键集合必须与权威文件逐项相同(整块搬运,不增不减)"
    foreach ($key in $expectedKeys) {
        Assert-Equal -Expected $authoritative[$key] -Actual $generated[$key] -Because "$ConfigMapName 的 $key 必须与 bin/etc/base_deploy_config.yaml 一致"
    }
}

Test-Case "C++ gRPC deadline 预算:仓库配置满足 deadline ≥ 目标 Go 服务 zrpc Timeout + 2000,五个目标全部显式配置(部署门禁对当前配置放行)" {
    # 不满足时 Assert 抛出的文本就是失败原因,逐项点名是哪个目标、差多少。
    $RepoRoot = Get-RepoRoot
    Assert-GrpcClientDeadlineBudget | Out-Null
}

Test-Case "zone-up 写路径入口先过 deadline 预算门禁,之后才有任何 kubectl 写操作" {
    $okAt = $devOut.IndexOf('GrpcClient deadline budget OK')
    $firstKubectlAt = $devOut.IndexOf('[dry-run] kubectl')
    Assert-True -Condition ($okAt -ge 0) -Because 'zone-up DryRun 必须经过 Assert-GrpcClientDeadlineBudget(写路径入口门禁),否则守卫等于没挂'
    Assert-True -Condition ($firstKubectlAt -lt 0 -or $okAt -lt $firstKubectlAt) -Because '预算核对必须先于任何 kubectl 写操作,不留半截部署'
}

Test-Case "node ConfigMap 的 GrpcClient.CallDeadlineMs 逐项 == bin/etc/base_deploy_config.yaml(ConfigMap 遮蔽镜像,漏搬 = K8s 上全部落回默认 10000)" {
    Assert-GrpcClientBlockMirrored -Output $devOut -ConfigMapName 'node-config'
}

Test-Case "K8s 生成物同样满足 deadline 预算:node-config 的 GrpcClient × zone 内 go-svc ConfigMap 实际写出的 Timeout(login 镜像 login.yaml、data-service 不写)" {
    # 部署门禁比对的是服务 yaml;这里再按集群里真正生效的那份核一遍不等式,防的是生成器与门禁悄悄分家:
    # scene-manager / login 的 Timeout 镜像被改回常数(login 以前就是模板里写死 100000),或 data-service 的 ConfigMap
    # 某天写出一个服务 yaml 里没有的 Timeout(它现在不写 = go-zero 默认 2000)。ConfigMap 键改名会让 Timeout 落回
    # 默认 2000、本条反而放行 —— 那一类由上面 login / scene-manager 的「ConfigMap 值 == 服务 yaml」逐键用例兜住(查不到键即失败)。
    # 全局服务(match / 路由服)不在 zone-up 产物里,它们的 ConfigMap Timeout 由 Get-AuthoritativeScalar 镜像服务 yaml,已被上面的门禁用例覆盖。
    $deployScalars = ConvertTo-UnprefixedScalars -Scalars (ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'node-config')).Scalars -Prefix 'data.base_deploy_config.yaml.'
    $targets = [ordered]@{}
    foreach ($t in @(
        @{ Target = 'SceneManagerNodeService'; ConfigMap = 'go-svc-scene-manager-config'; Prefix = 'data.scene_manager_service.yaml.' }
        @{ Target = 'LoginNodeService';        ConfigMap = 'go-svc-login-config';         Prefix = 'data.login.yaml.' }
        @{ Target = 'DataServiceNodeService';  ConfigMap = 'go-svc-data-service-config';  Prefix = 'data.data_service.yaml.' }
    )) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name $t.ConfigMap)
        $targets[$t.Target] = @{ Source = "K8s ConfigMap $($t.ConfigMap)"; Scalars = (ConvertTo-UnprefixedScalars -Scalars $flat.Scalars -Prefix $t.Prefix) }
    }
    $violations = @(Get-GrpcClientDeadlineBudgetViolations -DeployScalars $deployScalars -Targets $targets)
    Assert-True -Condition ($violations.Count -eq 0) -Because ("K8s 生成物违例:{0}" -f ($violations -join ';'))
}

Test-Case "自检:deadline 预算判定对 修复前的 2500 / 差 1ms / 缺席 / 0 / 非数字 / 服务端 Timeout 0 / 非数字 报违例并点名目标,对等号边界通过(守卫不得静默放行)" {
    $check = {
        param([string]$DeployText, [string]$ServiceText)
        $targets = [ordered]@{
            DataServiceNodeService = @{ Source = 'fixture/data_service.yaml'; Scalars = (ConvertFrom-YamlToFlatMap -Text $ServiceText).Scalars }
        }
        return @(Get-GrpcClientDeadlineBudgetViolations -DeployScalars (ConvertFrom-YamlToFlatMap -Text $DeployText).Scalars -Targets $targets)
    }
    $deployWith = "GrpcClient:`n  CallDeadlineMs:`n    DataServiceNodeService: "

    $passes = @(
        @{ Why = '4000 + 未写 Timeout(go-zero 默认 2000,等号边界)'; Deploy = "${deployWith}4000"; Service = "Name: dataservice.rpc" }
        @{ Why = '10000 + Timeout 8000(等号边界)'; Deploy = "${deployWith}10000"; Service = "Timeout: 8000" }
    )
    foreach ($c in $passes) {
        $got = @(& $check $c.Deploy $c.Service)
        Assert-True -Condition ($got.Count -eq 0) -Because ("{0} 应通过,实际报:{1}" -f $c.Why, ($got -join ';'))
    }

    $violationCases = @(
        @{ Why = '修复前的仓库值 2500 + 未写 Timeout(2500 < 2000 + 2000)'; Deploy = "${deployWith}2500"; Service = "Name: dataservice.rpc" }
        @{ Why = '差 1ms(余量是 2000)'; Deploy = "${deployWith}9999"; Service = "Timeout: 8000" }
        @{ Why = 'deadline 缺席(C++ 落到内置默认,预算不能依赖隐式值)'; Deploy = "GrpcClient:`n  CallDeadlineMs:`n    EtcdNodeService: 5000"; Service = "Timeout: 1000" }
        @{ Why = 'deadline 0(C++ 忽略后按默认)'; Deploy = "${deployWith}0"; Service = "Timeout: 1000" }
        @{ Why = 'deadline 非数字'; Deploy = "${deployWith}4s"; Service = "Timeout: 1000" }
        @{ Why = '服务端 Timeout 0(go-zero 不装超时拦截器)'; Deploy = "${deployWith}10000"; Service = "Timeout: 0" }
        @{ Why = '服务端 Timeout 非数字'; Deploy = "${deployWith}10000"; Service = "Timeout: 8s" }
    )
    foreach ($c in $violationCases) {
        $got = @(& $check $c.Deploy $c.Service)
        Assert-True -Condition ($got.Count -gt 0) -Because "$($c.Why) 必须报违例"
        Assert-Match -Text $got[0] -Pattern '^DataServiceNodeService:' -Because "$($c.Why) 的违例必须点名是哪个目标"
    }
}

# go-zero 的 MethodTimeouts 能把个别方法的服务端超时单独放宽。门禁逐条核对:C++ 会调用的方法照样要满足
# 「deadline ≥ 该方法超时 + 2000」;只有登记为「C++ 不调用」的方法(Get-GrpcClientMethodsNotCalledFromCpp)可以更高。
# 2026-10-09 之前的口径是「出现 MethodTimeouts 就拒绝」,它与 data_service 回档三个 RPC 的 MethodTimeouts 撞在一起,
# 使所有写路径在入口被拒(docs/handoff/repo-layout-20261007.md §6.3)。
Test-Case "自检:MethodTimeouts 逐条核对,C++ 会调用的方法必须在预算内,登记为 C++ 不调用的方法才可以更高(守卫不得静默放行)" {
    $check = {
        param([string]$Deadline, [string]$ServiceText, [string[]]$NotCalledFromCpp = @(), [switch]$OmitParsedEntries)
        $target = @{
            Source                  = 'fixture/data_service.yaml'
            Scalars                 = (ConvertFrom-YamlToFlatMap -Text $ServiceText).Scalars
            MethodsNotCalledFromCpp = $NotCalledFromCpp
        }
        if (-not $OmitParsedEntries) { $target['MethodTimeouts'] = @(Get-ZrpcMethodTimeouts -Text $ServiceText) }
        $deployText = "GrpcClient:`n  CallDeadlineMs:`n    DataServiceNodeService: $Deadline"
        return @(Get-GrpcClientDeadlineBudgetViolations -DeployScalars (ConvertFrom-YamlToFlatMap -Text $deployText).Scalars -Targets ([ordered]@{ DataServiceNodeService = $target }))
    }
    $allocate = '/data_service.DataService/AllocateIdSegment'
    $rollback = '/data_service.DataService/RollbackPlayer'
    $block = {
        param([string[]]$Lines)
        return "MethodTimeouts:`n" + ($Lines -join "`n")
    }
    $entry = { param([string]$Method, [string]$Timeout) return "  - FullMethod: $Method`n    Timeout: $Timeout" }

    $passes = @(
        @{ Why = '等号边界:8s + 2000 = 10000'; Deadline = '10000'; Service = (& $block (& $entry $allocate '8s')) }
        @{ Why = '小数时长不多算:16.1s 恰好是 16100,加 2000 等于 18100'; Deadline = '18100'; Service = (& $block (& $entry $allocate '16.1s')) }
        @{ Why = '逐方法超时比全局默认还短'; Deadline = '4000'; Service = (& $block (& $entry $allocate '500ms')) }
        @{ Why = '登记为 C++ 不调用的方法可以远高于 deadline'; Deadline = '4000'; Service = (& $block (& $entry $rollback '14400s')); NotCalled = @($rollback) }
        @{ Why = '豁免只放行登记的那一条,另一条照常在预算内'; Deadline = '10000'; Service = (& $block @((& $entry $rollback '300s'), (& $entry $allocate '8s'))); NotCalled = @($rollback) }
        @{ Why = '显式空表 MethodTimeouts: []'; Deadline = '4000'; Service = 'MethodTimeouts: []' }
        @{ Why = '键名小写(go-zero 配置键不分大小写)且在预算内'; Deadline = '10000'; Service = "methodTimeouts:`n  - fullMethod: $allocate`n    timeout: 8s" }
    )
    foreach ($c in $passes) {
        $notCalled = if ($c.ContainsKey('NotCalled')) { $c.NotCalled } else { @() }
        $got = @(& $check $c.Deadline $c.Service $notCalled)
        Assert-True -Condition ($got.Count -eq 0) -Because ("{0} 应通过,实际报:{1}" -f $c.Why, ($got -join ';'))
    }

    $violationCases = @(
        @{ Why = '差 1ms(8s + 2000 > 9999)'; Deadline = '9999'; Service = (& $block (& $entry $allocate '8s')); Pattern = 'AllocateIdSegment 8s' }
        @{ Why = '回档方法没登记豁免就不放行,并提示登记到哪'; Deadline = '4000'; Service = (& $block (& $entry $rollback '300s')); Pattern = 'RollbackPlayer 300s.*Get-GrpcClientMethodsNotCalledFromCpp' }
        @{ Why = '登记的是别的方法,这一条不沾光'; Deadline = '4000'; Service = (& $block (& $entry $allocate '300s')); NotCalled = @($rollback); Pattern = 'AllocateIdSegment 300s' }
        @{ Why = '豁免逐字匹配,大小写不同不算(go-zero 按 FullMethod 原样查表)'; Deadline = '4000'; Service = (& $block (& $entry $rollback '300s')); NotCalled = @($rollback.ToLowerInvariant()); Pattern = 'RollbackPlayer 300s' }
        @{ Why = 'Timeout 是裸数字(go-zero 加载配置时直接报错,几乎一定是把毫秒写错了地方)'; Deadline = '10000'; Service = (& $block (& $entry $allocate '8000')); Pattern = "AllocateIdSegment 的 Timeout='8000' 解析不了" }
        @{ Why = 'Timeout 为 0'; Deadline = '10000'; Service = (& $block (& $entry $allocate '0s')); Pattern = '解析不了' }
        @{ Why = 'Timeout 单位大写(Go 不认)'; Deadline = '10000'; Service = (& $block (& $entry $allocate '8S')); Pattern = '解析不了' }
        @{ Why = 'FullMethod 不是 /包.服务/方法'; Deadline = '10000'; Service = (& $block (& $entry 'AllocateIdSegment' '1s')); Pattern = "FullMethod='AllocateIdSegment' 不是" }
        @{ Why = '键名小写同样被核到(否则换个大小写就绕过门禁)'; Deadline = '4000'; Service = "methodTimeouts:`n  - fullMethod: $allocate`n    timeout: 300s"; Pattern = 'AllocateIdSegment 300s' }
        @{ Why = '豁免不跳过全局 Timeout 的核对'; Deadline = '9999'; Service = ("Timeout: 8000`n" + (& $block (& $entry $rollback '300s'))); NotCalled = @($rollback); Pattern = 'Timeout 8000 \+ 2000 = 10000' }
        @{ Why = '豁免只免比大小,不免格式:豁免条目的超时写坏了也要报'; Deadline = '4000'; Service = (& $block (& $entry $rollback 'abc')); NotCalled = @($rollback); Pattern = "RollbackPlayer 的 Timeout='abc' 解析不了" }
        @{ Why = '豁免条目的超时是裸数字同样要报'; Deadline = '4000'; Service = (& $block (& $entry $rollback '3600')); NotCalled = @($rollback); Pattern = "RollbackPlayer 的 Timeout='3600' 解析不了" }
        @{ Why = '有这个键却一条也没有(键后什么都没写)'; Deadline = '4000'; Service = "MethodTimeouts:`nName: x"; Pattern = '有 MethodTimeouts 键却没有解析出任何条目' }
    )
    foreach ($c in $violationCases) {
        $notCalled = if ($c.ContainsKey('NotCalled')) { $c.NotCalled } else { @() }
        $got = @(& $check $c.Deadline $c.Service $notCalled)
        Assert-True -Condition ($got.Count -gt 0) -Because "$($c.Why) 必须报违例"
        Assert-Match -Text $got[0] -Pattern '^DataServiceNodeService:' -Because "$($c.Why) 的违例必须点名是哪个目标"
        Assert-Match -Text ($got -join "`n") -Pattern $c.Pattern -Because "$($c.Why) 的违例必须说清是哪一条、为什么"
    }

    # 服务 yaml 里明明有 MethodTimeouts,调用方却没把逐方法解析结果交进来:不能当作没有,必须拒绝。
    $got = @(& $check '10000' (& $block (& $entry $allocate '8s')) @() -OmitParsedEntries)
    Assert-True -Condition ($got.Count -gt 0) -Because '有 MethodTimeouts 却没给逐方法解析结果必须报违例'
    Assert-Match -Text $got[0] -Pattern '^DataServiceNodeService:.*没有拿到逐方法的解析结果' -Because '违例要说明缺的是逐方法解析结果'
    $got = @(& $check '10000' "methodTimeouts: [{FullMethod: $allocate, Timeout: 1h}]" @() -OmitParsedEntries)
    Assert-True -Condition ($got.Count -gt 0) -Because '键名小写时同样要认出「有 MethodTimeouts 却没给逐方法解析结果」(go-zero 的键不分大小写)'
    Assert-Match -Text $got[0] -Pattern '^DataServiceNodeService:.*没有拿到逐方法的解析结果' -Because '小写键的违例同样要说明缺的是逐方法解析结果'

    # 下面三种是「两套解析对不上」的情形,直接喂构造值(绕开 Get-ZrpcMethodTimeouts,它自己会先 throw)。
    $deployScalars = (ConvertFrom-YamlToFlatMap -Text "GrpcClient:`n  CallDeadlineMs:`n    DataServiceNodeService: 4000").Scalars
    $violationsFor = {
        param([string]$ServiceText, [object[]]$Entries)
        $target = @{
            Source                  = 'fixture/data_service.yaml'
            Scalars                 = (ConvertFrom-YamlToFlatMap -Text $ServiceText).Scalars
            MethodTimeouts          = $Entries
            MethodsNotCalledFromCpp = @($rollback)
        }
        return @(Get-GrpcClientDeadlineBudgetViolations -DeployScalars $deployScalars -Targets ([ordered]@{ DataServiceNodeService = $target }))
    }

    # 条数对不上(解析器漏读 / 调用方传错文件)。
    $got = @(& $violationsFor (& $block @((& $entry $rollback '300s'), (& $entry $allocate '300s'))) @(@{ FullMethod = $rollback; Timeout = '300s' }))
    Assert-True -Condition ($got.Count -gt 0) -Because '解析出 1 条而拍平表里有 2 条必须报违例(否则漏掉的那条 AllocateIdSegment 300s 就被放过了)'
    Assert-Match -Text $got[0] -Pattern '解析出 1 条,拍平表里数到 2 条' -Because '违例要给出两边的条数'

    # 交进来了逐方法条目,拍平表里却没有这个键:条目不能因此不被核对。
    $got = @(& $violationsFor 'Name: x' @(@{ FullMethod = $allocate; Timeout = '300s' }))
    Assert-True -Condition ($got.Count -gt 0) -Because '有逐方法条目而拍平表里没有 MethodTimeouts 键必须报违例(不能让这条 300s 不经核对)'
    Assert-Match -Text $got[0] -Pattern '解析出 1 条,拍平表里数到 0 条' -Because '违例要给出两边的条数'

    # 带引号的键 + 流式写法:拍平表认得出键、数不出条目,解析结果也是 0 条。0 == 0 不能算通过
    # (go-zero 走完整的 YAML 解析,这张表在服务端照常生效;评审时抓到的误放)。
    $flowText = "`"MethodTimeouts`": [{FullMethod: $allocate, Timeout: 1h}]"
    $got = @(& $violationsFor $flowText @())
    Assert-True -Condition ($got.Count -gt 0) -Because '有 MethodTimeouts 键、两边都数出 0 条、值又不是 [] 时必须报违例,不能当成空表放行'
    Assert-Match -Text $got[0] -Pattern '有 MethodTimeouts 键却没有解析出任何条目' -Because '违例要说明是有键却读不出条目'
}

Test-Case "自检:Get-ZrpcMethodTimeouts 读得出块式写法的每一条,读不懂的一律 throw(不拿读错的表去核预算)" {
    $text = @(
        'Name: dataservice.rpc'
        'MethodTimeouts:  # 逐方法超时'
        '  - FullMethod: /a.B/One   # 行尾注释'
        '    Timeout: 300s'
        ''
        '  # 两个键的先后顺序可以反过来,值可以带引号'
        '  - Timeout: "1m30s"'
        "    FullMethod: '/a.B/Two'"
        'Redis:'
        '  Timeout: 5s'
        '  FullMethod: /not.A/Method'
    ) -join "`n"
    foreach ($variant in @($text, ($text -replace "`n", "`r`n"))) {
        $got = @(Get-ZrpcMethodTimeouts -Text $variant)
        Assert-Equal -Expected 2 -Actual $got.Count -Because '应当读出两条(LF 与 CRLF 结果相同),块在下一个顶层键 Redis 处结束'
        Assert-Equal -Expected '/a.B/One' -Actual $got[0].FullMethod -Because '第一条的方法名'
        Assert-Equal -Expected '300s' -Actual $got[0].Timeout -Because '第一条的超时(行尾注释要去掉)'
        Assert-Equal -Expected '/a.B/Two' -Actual $got[1].FullMethod -Because '第二条的方法名(引号要去掉)'
        Assert-Equal -Expected '1m30s' -Actual $got[1].Timeout -Because '第二条的超时与方法名各归各位,不能串到别的条目上'
    }

    $lower = @(Get-ZrpcMethodTimeouts -Text "methodtimeouts:`n  - fullmethod: /a.B/One`n    timeout: 1s")
    Assert-Equal -Expected '/a.B/One=1s' -Actual ("{0}={1}" -f $lower[0].FullMethod, $lower[0].Timeout) -Because '键名不分大小写(与 go-zero 的配置加载一致)'
    Assert-Equal -Expected 0 -Actual @(Get-ZrpcMethodTimeouts -Text "Name: x`nTimeout: 8000").Count -Because '没有这个键 = 空'
    Assert-Equal -Expected 0 -Actual @(Get-ZrpcMethodTimeouts -Text 'MethodTimeouts: []').Count -Because '显式空表 = 空'
    Assert-Equal -Expected 0 -Actual @(Get-ZrpcMethodTimeouts -Text '').Count -Because '空文本 = 空'
    Assert-Equal -Expected 0 -Actual @(Get-ZrpcMethodTimeouts -Text "Name: x  # 注释里提到 MethodTimeouts 不算`n# MethodTimeouts: [{FullMethod: /a.B/One, Timeout: 1h}]").Count -Because '只在注释里出现不算'

    $one = "  - FullMethod: /a.B/One`n    Timeout: 1h"
    $malformed = @(
        @{ Why = '流式写法'; Text = 'MethodTimeouts: [{FullMethod: /a.B/One, Timeout: 1s}]'; Pattern = '只认块式写法' }
        @{ Why = '条目里有未知键'; Text = "MethodTimeouts:`n  - FullMethod: /a.B/One`n    Timeout: 1s`n    Retries: 3"; Pattern = '未知键 Retries' }
        @{ Why = '同一条目里键重复'; Text = "MethodTimeouts:`n  - FullMethod: /a.B/One`n    Timeout: 1s`n    Timeout: 2s"; Pattern = 'Timeout 写了两次' }
        @{ Why = '条目缺 Timeout'; Text = "MethodTimeouts:`n  - FullMethod: /a.B/One"; Pattern = '缺 FullMethod 或 Timeout' }
        @{ Why = '条目缺 FullMethod'; Text = "MethodTimeouts:`n  - Timeout: 1s"; Pattern = '缺 FullMethod 或 Timeout' }
        @{ Why = '条目不以 - 开头'; Text = "MethodTimeouts:`n  FullMethod: /a.B/One`n  Timeout: 1s"; Pattern = "必须以 '- ' 开头" }
        @{ Why = '列表项是标量'; Text = "MethodTimeouts:`n  - just-a-string"; Pattern = '看不懂的行' }
        @{ Why = '键只有名字没有值'; Text = "MethodTimeouts:`n  - FullMethod:`n    Timeout: 1s"; Pattern = '看不懂的行' }
        @{ Why = '顶层出现两次'; Text = "MethodTimeouts:`n$one`nName: x`nMethodTimeouts:`n$one"; Pattern = '不止一次' }
        # 以下几种这里都读不出来,必须拒绝而不能当作没有这张表。除「嵌在别的键下面」外,go-zero 都会让它们照常生效
        # (它走完整的 YAML 解析);嵌套的那种在服务端并不生效,但这里不去分辨。
        @{ Why = '列表项顶格写'; Text = "MethodTimeouts:`n- FullMethod: /a.B/One`n  Timeout: 1s`nNext: 1"; Pattern = '列表项要缩进书写' }
        @{ Why = '键带双引号 + 流式写法'; Text = "Name: x`n`"MethodTimeouts`": [{`"FullMethod`": `"/a.B/One`", `"Timeout`": `"1h`"}]"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '键带单引号 + 块式写法'; Text = "'MethodTimeouts':`n$one"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '键带引号 + 顶格列表'; Text = "`"MethodTimeouts`":`n- Timeout: 1h`n  FullMethod: /a.B/One"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '整份文档统一缩进'; Text = "  Name: x`n  MethodTimeouts: [{FullMethod: /a.B/One, Timeout: 1h}]"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '嵌在别的键下面'; Text = "Outer:`n  MethodTimeouts:`n    - FullMethod: /a.B/One`n      Timeout: 1h"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '标准写法之后又用带引号的键写了一遍(YAML 里后者覆盖前者)'; Text = "MethodTimeouts:`n$one`n`"MethodTimeouts`": [{FullMethod: /a.B/Two, Timeout: 2h}]"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '标准写法之后又用大小写不同的带引号键写了一遍(go-zero 的键不分大小写,两条会归并)'; Text = "MethodTimeouts:`n$one`n`"methodTimeouts`": [{FullMethod: /a.B/Two, Timeout: 2h}]"; Pattern = '提到了 MethodTimeouts' }
        @{ Why = '整份文档写成一行流式映射 / JSON(键在行中间)'; Text = '{"Name": "x", "MethodTimeouts": [{"FullMethod": "/a.B/One", "Timeout": "1h"}]}'; Pattern = '提到了 MethodTimeouts' }
        # yaml.v2 把单独的 CR 与 NEL / LS / PS 也当换行;这里只按 LF / CRLF 切行,注释后面跟一个这样的字符,后续内容会被当成注释漏读。
        @{ Why = '行尾是单独的 CR'; Text = "Name: x # c`rMethodTimeouts:`r  - FullMethod: /a.B/One`r    Timeout: 1h`r"; Pattern = '单独的 CR 或 NEL / LS / PS' }
        @{ Why = '注释后面跟 U+2028(LS)'; Text = "Name: x # c$([char]0x2028)MethodTimeouts:`n$one"; Pattern = '单独的 CR 或 NEL / LS / PS' }
        @{ Why = '注释后面跟 U+0085(NEL)'; Text = "Name: x # c$([char]0x85)MethodTimeouts:`n$one"; Pattern = '单独的 CR 或 NEL / LS / PS' }
        @{ Why = '块内条目之间用 U+2029(PS)分隔'; Text = "MethodTimeouts:`n  - FullMethod: /a.B/One`n    Timeout: 1h # c$([char]0x2029)  - FullMethod: /a.B/Two`n    Timeout: 2h"; Pattern = '单独的 CR 或 NEL / LS / PS' }
    )
    foreach ($c in $malformed) {
        $message = ''
        try { Get-ZrpcMethodTimeouts -Text $c.Text | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern $c.Pattern -Because "$($c.Why) 必须 throw 并说清原因,不能读出一份残缺的表"
    }
}

Test-Case "自检:ConvertTo-ZrpcDurationMs 认的是 Go 时长写法的子集,换算不多算,解析不了返回空(交给调用方报违例)" {
    $expected = [ordered]@{
        '300s'      = 300000
        '3600s'     = 3600000
        '14400s'    = 14400000
        '1m30s'     = 90000
        '1h2m3s4ms' = 3723004
        '500ms'     = 500
        '1.5s'      = 1500
        '16.1s'     = 16100
        '2.007s'    = 2007
        '2h'        = 7200000
        ' 8s '      = 8000
        '0.4ms'     = 1
    }
    foreach ($text in $expected.Keys) {
        Assert-Equal -Expected $expected[$text] -Actual (ConvertTo-ZrpcDurationMs -Text $text) -Because "'$text' 的毫秒数(小数不能多算,不足 1ms 向上取整)"
    }
    $fullWidthThree = [string][char]0xFF13
    foreach ($text in @('3000', '8S', '1m30S', '1H30m', '0s', '0ms', '-5s', '+5s', '.5s', '5.s', '1e3s', '1d', '10us', '5 s', 's', '', 'abc', '5s extra', "${fullWidthThree}s", "${fullWidthThree}s5s", "5s${fullWidthThree}ms", '99999999999999999999999999h')) {
        Assert-True -Condition ($null -eq (ConvertTo-ZrpcDurationMs -Text $text)) -Because "'$text' 不是带单位的正时长(单位只认小写的 ms / s / m / h,数字只认 0-9),必须返回空而不是抛异常"
    }
}

# 部署门禁读的全部文件。两条夹具用例都把它们拷到临时目录再改,不碰仓库。
$deadlineBudgetFixtureFiles = @(
    'bin/etc/base_deploy_config.yaml'
    'go/scene_manager/etc/scene_manager_service.yaml'
    'go/data_service/etc/data_service.yaml'
    'go/client_rpc_router/etc/client_rpc_router.yaml'
    'go/match/etc/match_service.yaml'
    'go/login/etc/login.yaml'
)

Test-Case "负向:DataService deadline 退回修复前的 2500 时部署门禁必须 throw 并只点名该项(夹具是临时目录里的配置副本,不碰仓库)" {
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('grpc-deadline-budget-' + [guid]::NewGuid().ToString('N'))
    try {
        foreach ($rel in $deadlineBudgetFixtureFiles) {
            $dst = Join-Path $tempRoot $rel
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
            Copy-Item -LiteralPath (Join-Path (Get-RepoRoot) $rel) -Destination $dst
        }
        $deployCopy = Join-Path $tempRoot 'bin/etc/base_deploy_config.yaml'
        $original = Get-Content -LiteralPath $deployCopy -Raw
        $mutated = [regex]::Replace($original, '(?m)^(\s+DataServiceNodeService:\s*)\d+', '${1}2500')
        Assert-True -Condition ($mutated -ne $original) -Because '夹具必须真的把 DataServiceNodeService 改成 2500,否则下面的负向断言是空转'
        [System.IO.File]::WriteAllText($deployCopy, $mutated, [System.Text.UTF8Encoding]::new($false))

        $RepoRoot = $tempRoot
        $message = ''
        try { Assert-GrpcClientDeadlineBudget | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern '拒绝部署' -Because '预算不成立必须拒绝部署,不能只打警告'
        Assert-Match -Text $message -Pattern 'DataServiceNodeService:C\+\+ deadline 2500 < .*go-zero 默认 2000.* = 4000' -Because '错误必须点名目标、实际值与按 go-zero 默认 2000 算出的下限 4000'
        Assert-NotMatch -Text $message -Pattern 'SceneManagerNodeService|ClientRpcRouterNodeService|MatchNodeService|LoginNodeService' -Because '其余四项仍满足,只点名不满足的那一项'
    }
    finally {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Test-Case "负向:给 C++ 会调用的 AllocateIdSegment 配一条超预算的 MethodTimeouts,或把这张表写坏,部署门禁都必须 throw(夹具是临时目录里的配置副本,不碰仓库)" {
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('grpc-deadline-method-timeouts-' + [guid]::NewGuid().ToString('N'))
    try {
        foreach ($rel in $deadlineBudgetFixtureFiles) {
            $dst = Join-Path $tempRoot $rel
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
            Copy-Item -LiteralPath (Join-Path (Get-RepoRoot) $rel) -Destination $dst
        }
        # 把副本里的 deadline 钉成 4000:本用例验的是判定逻辑,不该随仓库里这个值的调整变红。
        $deployCopy = Join-Path $tempRoot 'bin/etc/base_deploy_config.yaml'
        $deployText = Get-Content -LiteralPath $deployCopy -Raw
        Assert-Match -Text $deployText -Pattern '(?m)^\s+DataServiceNodeService:\s*\d+' -Because '夹具前提:base_deploy_config.yaml 里要有 DataServiceNodeService 的 deadline'
        [System.IO.File]::WriteAllText($deployCopy, [regex]::Replace($deployText, '(?m)^(\s+DataServiceNodeService:\s*)\d+', '${1}4000'), [System.Text.UTF8Encoding]::new($false))

        $serviceCopy = Join-Path $tempRoot 'go/data_service/etc/data_service.yaml'
        $original = Get-Content -LiteralPath $serviceCopy -Raw
        # 与解析器同样宽:键名不分大小写,键后可以带行尾注释。否则有人只给这一行加条注释,门禁照常放行、本用例却假红。
        $keyLine = '(?mi)^MethodTimeouts[ \t]*:[ \t]*(#[^\r\n]*)?\r?$'
        Assert-Match -Text $original -Pattern $keyLine -Because '夹具前提:data_service.yaml 里要有块式的 MethodTimeouts(回档三条),下面两处改动都插在它的第一行之后'
        $RepoRoot = $tempRoot

        # 前提:副本不改就该放行。否则下面几条负向断言分不清是谁造成的失败。
        Assert-GrpcClientDeadlineBudget | Out-Null

        # 通过行只列真正按登记表放行的条目:预算内的普通条目、以及与登记项只差大小写的方法(判定函数不豁免它,
        # 只是 1s + 2000 ≤ 4000 所以通过)都不能被写成 exempt,否则部署的人看到的豁免清单是错的。
        $withInBudget = [regex]::Replace($original, $keyLine, "MethodTimeouts:`n  - FullMethod: /data_service.DataService/AllocateIdSegment`n    Timeout: 1s`n  - FullMethod: /data_service.DataService/rollbackplayer`n    Timeout: 1s")
        [System.IO.File]::WriteAllText($serviceCopy, $withInBudget, [System.Text.UTF8Encoding]::new($false))
        $passLine = [string](Assert-GrpcClientDeadlineBudget 6>&1 | Out-String)
        Assert-Match -Text $passLine -Pattern 'GrpcClient deadline budget OK' -Because '两条预算内的条目不该让门禁拒绝(夹具前提)'
        Assert-Match -Text $passLine -Pattern 'RollbackPlayer=' -Because '登记过的回档条目仍要出现在豁免清单里'
        Assert-NotMatch -Text $passLine -Pattern 'AllocateIdSegment=' -Because '没登记的条目是经过 deadline 比较才通过的,不能出现在豁免清单里'
        Assert-True -Condition (-not ($passLine -cmatch 'rollbackplayer=')) -Because '与登记项只差大小写的方法没有被豁免,不能出现在豁免清单里(登记表逐字匹配)'

        $withAllocate = [regex]::Replace($original, $keyLine, "MethodTimeouts:`n  - FullMethod: /data_service.DataService/AllocateIdSegment`n    Timeout: 8s")
        [System.IO.File]::WriteAllText($serviceCopy, $withAllocate, [System.Text.UTF8Encoding]::new($false))
        $message = ''
        try { Assert-GrpcClientDeadlineBudget | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern '拒绝部署' -Because 'C++ 会调用的方法被放宽到 deadline 之上必须拒绝部署'
        Assert-Match -Text $message -Pattern 'DataServiceNodeService:C\+\+ deadline 4000 < .*AllocateIdSegment 8s.*= 10000' -Because '错误必须点名目标、方法与按 8s + 2000 算出的下限'
        Assert-NotMatch -Text $message -Pattern 'Rollback(Player|Zone|All)' -Because '回档三条已登记为 C++ 不调用,不应被点名'
        Assert-NotMatch -Text $message -Pattern 'SceneManagerNodeService|ClientRpcRouterNodeService|MatchNodeService|LoginNodeService' -Because '其余四项仍满足,只点名不满足的那一项'

        $withUnknownKey = [regex]::Replace($original, $keyLine, "MethodTimeouts:`n  - FullMethod: /data_service.DataService/RollbackPlayer`n    Timeout: 300s`n    Retries: 3")
        [System.IO.File]::WriteAllText($serviceCopy, $withUnknownKey, [System.Text.UTF8Encoding]::new($false))
        $message = ''
        try { Assert-GrpcClientDeadlineBudget | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern 'go/data_service/etc/data_service\.yaml 的 MethodTimeouts 解析不了.*未知键 Retries' -Because '读不懂的 MethodTimeouts 必须拒绝部署并点名文件与原因,不能当作没有这张表'

        # 评审时抓到的误放:在文件末尾用带引号的键、流式写法再写一遍,YAML 里后者覆盖前者,go-zero 照常生效。
        $withQuotedFlow = $original.TrimEnd() + "`n`"MethodTimeouts`": [{FullMethod: /data_service.DataService/AllocateIdSegment, Timeout: 1h}]`n"
        [System.IO.File]::WriteAllText($serviceCopy, $withQuotedFlow, [System.Text.UTF8Encoding]::new($false))
        $message = ''
        try { Assert-GrpcClientDeadlineBudget | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern 'go/data_service/etc/data_service\.yaml 的 MethodTimeouts 解析不了.*提到了 MethodTimeouts' -Because '带引号的键 + 流式写法必须拒绝部署,不能因为两套解析都读不出条目就放行'
    }
    finally {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Test-Case "通过时回显按「C++ 不调用」放行的 MethodTimeouts 条目(部署的人看得到这次豁免了什么)" {
    $okLine = [string]($devOut -split "`r?`n" | Where-Object { $_ -match 'GrpcClient deadline budget OK' } | Select-Object -First 1)
    Assert-Match -Text $okLine -Pattern 'MethodTimeouts exempt \(never called from C\+\+\): ' -Because 'zone-up 的输出里,门禁通过的那一行必须列出被豁免的条目'
    $table = Get-GrpcClientMethodsNotCalledFromCpp
    foreach ($target in $table.Keys) {
        foreach ($fullMethod in @($table[$target])) {
            Assert-Match -Text $okLine -Pattern ([regex]::Escape("$target $fullMethod=")) -Because "$fullMethod 在服务 yaml 里配了 MethodTimeouts 且已登记,通过时必须点名它"
        }
    }
}

Test-Case "「C++ 不调用」表名副其实:表里的方法 C++ 既不直接调用、gate 也不会按消息号转发,且每一项都对应服务 yaml 里的一条 MethodTimeouts(没有过期项)" {
    # 表里的目标 → 它的服务 yaml。新增目标时在这里补一行(与 Assert-GrpcClientDeadlineBudget 的 $targetSources 同值)。
    $serviceYamlOf = @{ DataServiceNodeService = 'go/data_service/etc/data_service.yaml' }

    # 手写 C++ = cpp/ 下除生成物(cpp/generated,生成客户端自身的定义就在那里)与内嵌 muduo 之外的全部源码。
    $handWritten = @(Get-ChildItem -LiteralPath (Join-Path (Get-RepoRoot) 'cpp') -Recurse -File | Where-Object {
        @('.cpp', '.cc', '.h', '.hpp') -contains $_.Extension -and (($_.FullName -replace '\\', '/') -notmatch '/cpp/generated/|/muduo_windows/')
    })
    Assert-True -Condition ($handWritten.Count -gt 100) -Because "手写 C++ 源文件应当有几百个,实际只找到 $($handWritten.Count) 个(搜索范围写错了?)"

    # gate 直连模式不写任何符号:它拿客户端消息号查 gRpcMethodRegistry 再调 .sender 转发
    # (cpp/nodes/gate/handler/rpc/client_message_processor.cpp)。哪些消息号算客户端消息,由生成的 IsClientMessageId 决定。
    $registryText = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'cpp/generated/rpc/service_metadata/rpc_event_registry.cpp') -Raw
    $clientIdsFunction = [regex]::Match($registryText, '(?s)bool\s+IsClientMessageId\s*\([^)]*\)\s*\{(?<body>.*?)\r?\n\}')
    Assert-True -Condition $clientIdsFunction.Success -Because 'rpc_event_registry.cpp 里应当有 IsClientMessageId(生成器改了形状的话,本用例的「gate 会转发」判据要跟着改)'
    $clientMessageIds = @([regex]::Matches($clientIdsFunction.Groups['body'].Value, '\bcase\s+(?<id>[A-Za-z_][A-Za-z0-9_]*MessageId)\s*:') | ForEach-Object { $_.Groups['id'].Value })
    Assert-True -Condition ($clientMessageIds.Count -gt 20) -Because "IsClientMessageId 里应当列着上百个客户端消息号,实际只读到 $($clientMessageIds.Count) 个(解析写错了?)"

    $namesOf = {
        param([string]$FullMethod)
        $parts = [regex]::Match($FullMethod, '^/(?:[A-Za-z_][A-Za-z0-9_]*\.)*(?<service>[A-Za-z_][A-Za-z0-9_]*)/(?<method>[A-Za-z_][A-Za-z0-9_]*)$')
        if (-not $parts.Success) { throw "全方法名不是 /包.服务/方法 的形状: $FullMethod" }
        return $parts.Groups['service'].Value + $parts.Groups['method'].Value
    }
    # 直接调用:生成客户端的符号 Send<服务><方法> / Async<服务><方法>[Handler|FailedHandler|GrpcClient],或它的消息号常量。
    $findDirectUses = {
        param([string]$FullMethod)
        $name = & $namesOf $FullMethod
        $symbol = '\b(?:(?:Send|Async)' + $name + '(?:Handler|FailedHandler|GrpcClient)?|' + $name + 'MessageId)\b'
        return @($handWritten | Select-String -Pattern $symbol -CaseSensitive | ForEach-Object { "{0}:{1}" -f ($_.Path -replace '\\', '/'), $_.LineNumber })
    }
    $isForwardedByGate = { param([string]$FullMethod) return ($clientMessageIds -ccontains ((& $namesOf $FullMethod) + 'MessageId')) }

    # 正向对照:两种调用形态各拿一个已知会被调用的方法验一遍。搜不到它们,说明搜索本身坏了,下面的「搜不到」不能算证据。
    Assert-True -Condition (@(& $findDirectUses '/data_service.DataService/AllocateIdSegment').Count -gt 0) -Because '搜索必须能找到已知的直接调用 SendDataServiceAllocateIdSegment(scene 的号段领取),否则本用例的结论无效'
    Assert-True -Condition (& $isForwardedByGate '/loginpb.ClientPlayerLogin/Login') -Because 'Login 是 gate 直连模式按消息号转发的客户端消息,必须被判成「C++ 会调用」,否则本用例对这种调用形态是瞎的'
    Assert-True -Condition (-not (& $isForwardedByGate '/data_service.DataService/AllocateIdSegment')) -Because 'AllocateIdSegment 不是客户端消息,gate 不转发它(反向对照:判据不能把所有方法都判成会转发)'

    $table = Get-GrpcClientMethodsNotCalledFromCpp
    Assert-True -Condition ($table.Count -gt 0) -Because '表不应为空(回档三个 RPC 登记在这里;若它们的 MethodTimeouts 已删,连同本表一起清掉并改写本断言)'
    foreach ($target in $table.Keys) {
        Assert-True -Condition $serviceYamlOf.ContainsKey($target) -Because "本用例不认识目标 ${target}:在 `$serviceYamlOf 里补上它的服务 yaml"
        $configured = @(Get-ZrpcMethodTimeouts -Text (Get-Content -LiteralPath (Join-Path (Get-RepoRoot) $serviceYamlOf[$target]) -Raw) | ForEach-Object { [string]$_.FullMethod })
        foreach ($fullMethod in @($table[$target])) {
            $uses = @(& $findDirectUses $fullMethod)
            Assert-True -Condition ($uses.Count -eq 0) -Because ("{0} 登记为 C++ 不调用,但手写 C++ 里出现了它的生成客户端符号或消息号:{1}。C++ 一旦调用它,就必须满足 deadline ≥ 该方法超时 + 2000,不能再豁免" -f $fullMethod, ($uses -join ', '))
            Assert-True -Condition (-not (& $isForwardedByGate $fullMethod)) -Because "$fullMethod 登记为 C++ 不调用,但它的消息号列在 IsClientMessageId 里:gate 直连模式会按消息号把它转发给目标服务,不能豁免"
            Assert-True -Condition ($configured -ccontains $fullMethod) -Because "$fullMethod 登记在表里,但 $($serviceYamlOf[$target]) 的 MethodTimeouts 里没有这一条:过期的豁免要删掉,免得将来给它配长超时时无人复核"
        }
    }
}

Test-Case "node ConfigMap 必须带 GateTokenSecret(否则 gate 在 prod 运行模式下拒绝启动)" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "node-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.GateTokenSecret'
    Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($v)) -Because "ValidateGateTokenSecretOrDie 在空密钥 + prod 下 LOG_FATAL,而部署链从不设 GATE_RUN_MODE(默认即 prod)"
}

Test-Case "node ConfigMap 的 NodeTTLSeconds 必须与 bin/etc 权威值一致" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "node-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.Etcd.NodeTTLSeconds'
    $authoritative = Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'Etcd.NodeTTLSeconds'
    Assert-Equal -Expected $authoritative -Actual $v -Because "生成器写死 60 会退回 2026-05-24 压测前的值,45k 浪涌下 keepalive 抖一帧就误判租约过期 FATAL"
}

Test-Case "node ConfigMap 必须带 GateMaxConnections(缺了等于 gate 无连接数上限)" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "node-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.GateMaxConnections'
    $authoritative = Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'GateMaxConnections'
    Assert-Equal -Expected $authoritative -Actual $v -Because "0/缺失 = 不限,而 gate 是唯一对公网开放的端口"
    Assert-True -Condition ([uint64]$v -gt 0) -Because "仓库默认/K8s 部署必须启用连接上限;0 只允许显式 dev/test 本地运行"
    Assert-True -Condition ([uint64]$v -le 131071) -Because "session id 低 17 位回绕;超过安全容量会让碰撞检查永久自旋"
}

Test-Case "node ConfigMap 的 AuditTopicGeneration 必须与 data-service 的 Kafka.TopicGeneration 一致" {
    # 审计 topic 的有效名 = `<基名>_g<世代号>`。C++ scene 是 transaction_log / player_snapshot
    # 唯一的生产者,data-service 是唯一的消费者,topic 名是两边唯一的会合点 —— 两边世代号分家
    # 不报任何错:生产者往一个没人消费的 topic 写,30 天保留期一到就静默没了。
    # 2026-09-09 前 C++ 侧后缀是编译期常量、这份 ConfigMap 刻意不写这一键;现在
    # config.cpp::readBaseDeployConfig 真读 AuditTopicGeneration,所以必须写,且必须等于消费者那份。
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "node-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.AuditTopicGeneration'
    $authoritative = Get-EtcValue -RelativePath 'go/data_service/etc/data_service.yaml' -KeyPath 'Kafka.TopicGeneration'
    Assert-Equal -Expected $authoritative -Actual $v -Because "生产者(C++)与消费者(data-service)的世代号不等 = 流水/快照写进没人消费的 topic,无任何报错"
}

Test-Case "node ConfigMap 的 DbTaskTopicGeneration 必须与 go/db、go/login 的 Kafka.TopicGeneration 三方一致" {
    # 玩家存盘 DBTask topic:C++ scene 按 home_zone 往 db_task_zone_{zone}[_g<N>] 写,go/db 是唯一的消费者,
    # login 也读写同一组 topic(player-storage-placement.md §7 / §12 A14)。三方世代号分家不报任何错 ——
    # 换代后 scene 仍写旧代 topic,存盘静默积压在已排空、没人消费的旧 topic 里。
    # config.cpp::readBaseDeployConfig 真读 DbTaskTopicGeneration,且 node ConfigMap 整目录遮蔽镜像里的
    # bin/etc,所以必须写,且必须等于消费者那份;同时钉住生成出来的 go-svc-db ConfigMap 那一份。
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "node-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.DbTaskTopicGeneration'
    $dbAuthoritative = Get-EtcValue -RelativePath 'go/db/etc/db.yaml' -KeyPath 'ServerConfig.Kafka.TopicGeneration'
    $loginAuthoritative = Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath 'Kafka.TopicGeneration'
    Assert-Equal -Expected $dbAuthoritative -Actual $v -Because "生产者(C++ scene)与消费者(go/db)的 db_task 世代号不等 = 存盘写进没人消费的 topic,无任何报错"
    Assert-Equal -Expected $dbAuthoritative -Actual $loginAuthoritative -Because "login 与 db 读写同一组 db_task topic,换代必须同一次改"
    $dbFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-db-config")
    Assert-Equal -Expected $v -Actual (Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.ServerConfig.Kafka.TopicGeneration') -Because "node 与 go-svc-db 两份 ConfigMap 必须从同一个键派生"
}

Test-Case "data-service 的 MappingRedis 不写 DB 键;go/db 读落点记录的 Redis 必须与它同址且为 DB 0" {
    # go-zero 的 RedisConf 没有 DB 字段:ConfigMap 里写 `DB: 15` 会被静默忽略,映射实际在 DB 0,
    # 留着只会让运维照它给 merge_zone 填 -mapping-redis-db 15(gap-fixes B6 / player-storage-placement.md §12 A12)。
    # go/db 按落点选库时 MGET player:placement / player:zone(§6.2):Placement.Redis 不写就复用 ServerConfig.RedisClient。
    # 两边不同址或不是 DB 0 = 读不到任何记录与 home_zone,全员按本 zone 选库,被钉到别处的玩家静默写错库。
    $dsFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-data-service-config")
    $mappingHost = Get-FlatValue -Flat $dsFlat -KeyPath 'data.data_service.yaml.MappingRedis.Host'
    Assert-True -Condition (-not $dsFlat.Scalars.Contains('data.data_service.yaml.MappingRedis.DB')) -Because "MappingRedis 的 DB 键对 go-zero 无效,写出来只会误导合服参数"

    $dbFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-db-config")
    # 显式写了 Placement.Redis 就以它为准(DB 缺省 0),否则取 RedisClient —— 与 go/db 的取值规则一致。
    if ($dbFlat.Scalars.Contains('data.db.yaml.Placement.Redis.Hosts')) {
        $placementHost = Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.Placement.Redis.Hosts'
        $placementDb = if ($dbFlat.Scalars.Contains('data.db.yaml.Placement.Redis.DB')) { Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.Placement.Redis.DB' } else { '0' }
    }
    else {
        $placementHost = Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.ServerConfig.RedisClient.Hosts'
        $placementDb = Get-FlatValue -Flat $dbFlat -KeyPath 'data.db.yaml.ServerConfig.RedisClient.DB'
    }
    Assert-Equal -Expected $mappingHost -Actual $placementHost -Because "go/db 读落点记录的 Redis 必须是 data-service 的 MappingRedis 实例"
    Assert-Equal -Expected '0' -Actual $placementDb -Because "落点记录与 player:zone 恒在 DB 0"
}

Test-Case "login ConfigMap 必须带 Secrets.InternalAuth(否则生产 login 拒绝启动)" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-login-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Secrets.InternalAuth.Value'
    Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($v)) -Because "生产模式 EnforceInternalAuth 恒 true 且关不掉,未配置时 ResolveSecrets 直接返回 error"
}

Test-Case "db ConfigMap 必须带非空的库名白名单(否则 db 在 strict 档下拒启)" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-db-config")
    $v = Get-FlatValue -Flat $flat -KeyPath 'data.db.yaml.ServerConfig.Database.AllowedDatabases[0]'
    Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($v)) -Because "三个来源全空时 AllowlistEnforcement 默认 strict = 拒启"
}

# ─────────────────────────────────────────────────────────────────
# 2. 镜像不可变版本戳
# ─────────────────────────────────────────────────────────────────

Test-Case "生成的所有 image: 引用都不得是 latest 等可变 tag" {
    $imageLines = @([regex]::Matches($devOut, '(?m)^\s*image:\s*(?<ref>\S+)\s*$') | ForEach-Object { $_.Groups['ref'].Value })
    Assert-True -Condition ($imageLines.Count -gt 0) -Because "DryRun 输出里应当有 image: 行"
    foreach ($ref in $imageLines) {
        $tag = Get-ImageTagFromRef -ImageRef $ref
        $chk = Test-ImmutableImageTag -Tag $tag
        Assert-True -Condition $chk.Ok -Because "镜像引用 '$ref' 用了可变 tag,rollout undo 会退到同一个 digest($($chk.Reason))"
    }
}

Test-Case "显式传可变 tag 时 imagePullPolicy 必须是 Always" {
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @("-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:latest"))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dev 档位允许可变 tag,只是要改 pullPolicy"
    Assert-Match -Text $run.Output -Pattern 'imagePullPolicy:\s*Always' -Because "可变 tag 配 IfNotPresent = 节点上有旧层就永远不拉新的"
}

Test-Case "不可变 tag 时 imagePullPolicy 是 IfNotPresent" {
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @("-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:0123456789ab"))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "基线 DryRun 应当成功"
    Assert-Match -Text $run.Output -Pattern 'imagePullPolicy:\s*IfNotPresent' -Because "内容不会变的 tag 没必要每次重拉"
}

# ─────────────────────────────────────────────────────────────────
# 3. prod 档位的密钥注入
# ─────────────────────────────────────────────────────────────────

$ProdEnv = @{
    MMORPG_GATE_TOKEN_SECRET  = 'contract-test-secret-0123456789abcdef0123'
    MMORPG_MYSQL_USER         = 'mmorpg_app'
    MMORPG_MYSQL_PASSWORD     = 'contract-test-mysql-pw-01'
    MMORPG_REDIS_PASSWORD     = 'contract-test-redis-pw-01'
    MMORPG_GATEWAY_DB_USER    = 'gateway_app'
    MMORPG_GATEWAY_DB_PASSWORD= 'contract-test-gwdb-pw-01'
    # login 的 Secrets.InternalAuth:生产模式恒强制验签且关不掉,缺了 login 拒启。
    # 刻意与 GATE_TOKEN_SECRET 不同 —— secrets.go 会拒绝跨用途复用主密钥。
    MMORPG_INTERNAL_AUTH_SECRET = 'contract-test-internal-auth-0123456789abcdef'
    # db 库名白名单:三个来源全空时 db 在默认 strict 档下拒启。
    MMORPG_DB_ALLOWED_DATABASES = 'zone_1_db,zone_2_db'
    # gateway 管理面口令(k8s-client-entry D91 纵深防御):非 dev 档缺失即拒绝部署;
    # 取 ≥32 位非占位串,且与上面各密钥都不同。
    MMORPG_GATEWAY_ADMIN_API_KEY = 'contract-test-gw-admin-key-0123456789abcdef'
    # login 登录排队令牌密钥(Secrets.QueueToken,G4b):staging / prod 缺省开排队,本次部署 login 时必填。
    # ≥32 位;与上面各值都不同(login 拒绝跨用途复用主密钥,部署侧对 GATE / INTERNAL_AUTH 两把同样拒绝);
    # 也不以 go/login/internal/config/secrets.go placeholderPrefixes 里的前缀开头(Go 侧按前缀判占位,比部署侧严)。
    MMORPG_LOGIN_QUEUE_TOKEN_SECRET = 'contract-test-queue-token-0123456789abcdef'
    # 登录排队合入门槛的测试接缝(2026-10-15,k8s_deploy.ps1 Resolve-LoginQueueSwitch):门槛未清($script:LoginQueueMergeGateCleared
    # 为 $false)时 staging / prod 的 auto 解析为关。设为 1 只在 -DryRun 下视为门槛已清,让下面依赖「排队开」的 DryRun
    # ($prodRun、$peakStagingRun、$peakAllUpRun,以及用例 10 / 11 由本表拷贝出的 $env2)照旧拿到排队开的产物;
    # 不带 -DryRun 时 k8s_deploy.ps1 见到它就拒绝,所以它放不开任何真实部署。本文件所有部署子进程都带 -DryRun。
    # 断「门槛未清」的用例 19 / 20 与不依赖排队的 $peakProdOffRun 在各自的拷贝里显式置空。
    MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED = '1'
}
$ProdArgs = $BaseArgs + @(
    "-ReleaseProfile", "prod",
    "-SkipPreflight",
    "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:0123456789ab"
)

$prodRun = Invoke-DeployDryRun -Arguments $ProdArgs -Env $ProdEnv

Test-Case "prod 档位 + 注入环境变量时生成器可以正常出产物" {
    Assert-Equal -Expected 0 -Actual $prodRun.ExitCode -Because "注入齐全时不该失败。输出: $($prodRun.Output)"
}

Test-Case "data-service ConfigMap 显式写 RouteStore.Mode: redis(dev / prod 两档),服务 yaml 也是 redis" {
    # 映射持久化第 1a 批(docs/design/player-route-mapping-durability-proposal.md §11.2):ConfigMap 不镜像服务 yaml,
    # 所有档位固定 redis。切 db_first 是 P2 的显式运维动作,不能因为谁改了本地 yaml 就被带进集群。
    foreach ($case in @(@{ Label = 'dev'; Output = $devOut }, @{ Label = 'prod'; Output = $prodRun.Output })) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $case.Output -Name 'go-svc-data-service-config')
        Assert-Equal -Expected 'redis' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.data_service.yaml.RouteStore.Mode') `
            -Because "$($case.Label):第 1a 批所有档位固定 redis;切 db_first 是 P2 的显式动作(冻结窗口 + 导入 + audit 为 0)"
    }
    Assert-Equal -Expected 'redis' -Actual (Get-EtcValue -RelativePath 'go/data_service/etc/data_service.yaml' -KeyPath 'RouteStore.Mode') `
        -Because '本地默认必须与今天逐字相同'
}

Test-Case "prod 档位下 db 段不得出现 root/root 或空密码" {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $prodRun.Output -Name "go-svc-db-config")
    $user = Get-FlatValue -Flat $flat -KeyPath 'data.db.yaml.ServerConfig.Database.User'
    $passwd = Get-FlatValue -Flat $flat -KeyPath 'data.db.yaml.ServerConfig.Database.Passwd'

    Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($passwd)) -Because "空密码 = 库随便连"
    Assert-True -Condition (-not (Test-PlaceholderSecret -Value $passwd)) -Because "prod 的 MySQL 密码不得是 'root' / '123456' 这类占位串(实际 '$passwd')"
    Assert-True -Condition (-not ($user -eq 'root' -and $passwd -eq 'root')) -Because "root/root 是生成器以前写死的值,在真集群上连都连不上"
    Assert-Equal -Expected $ProdEnv.MMORPG_MYSQL_PASSWORD -Actual $passwd -Because "密码必须来自环境变量注入"
}

Test-Case "prod 档位下所有 ConfigMap 都不得残留占位密钥串" {
    Assert-NotMatch -Text $prodRun.Output -Pattern 'change-me' -Because "生成器自己把占位常量写进生产 ConfigMap 是本轮要根除的问题"
    Assert-NotMatch -Text $prodRun.Output -Pattern 'password:\s*"?123456"?' -Because "Java Gateway 数据源密码以前写死 123456"
}

Test-Case "prod 档位下 login / scene-manager / gateway 的 GateTokenSecret 都来自同一个注入值" {
    foreach ($case in @(
        @{ Cm = 'go-svc-login-config';         Key = 'data.login.yaml.GateTokenSecret' }
        @{ Cm = 'go-svc-scene-manager-config'; Key = 'data.scene_manager_service.yaml.GateTokenSecret' }
        @{ Cm = 'java-svc-gateway-config';     Key = 'data.application.yaml.gate.token-secret' }
    )) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $prodRun.Output -Name $case.Cm)
        $v = Get-FlatValue -Flat $flat -KeyPath $case.Key
        Assert-Equal -Expected $ProdEnv.MMORPG_GATE_TOKEN_SECRET -Actual $v -Because "$($case.Cm) 的 gate 密钥必须来自 MMORPG_GATE_TOKEN_SECRET"
    }
}

Test-Case "login 的 go-zero Mode:dev 档必须写且 == go/login/etc/login.yaml,prod 档必须不写(保持 pro 门禁)" {
    # 背景(2026-09-03 kind B 档实跑):dev 档密钥是占位回落,login 的 secrets 门禁只在
    # Mode=dev/test 才降级为 WARN;ConfigMap 不写 Mode 就是默认 pro → login 起来即 panic
    # "HMAC 密钥配置不合格,拒绝启动"。修法是 dev 档写 Mode(值与 etc 权威文件一致),
    # 但 prod 档绝不能跟着写 —— 写了等于把生产的 fail-closed 门禁一起关掉。
    $devFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name "go-svc-login-config")
    $expected = Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath 'Mode'
    $actual = Get-FlatValue -Flat $devFlat -KeyPath 'data.login.yaml.Mode'
    Assert-Equal -Expected $expected -Actual $actual -Because "dev 档 login ConfigMap 的 Mode 必须与 go/login/etc/login.yaml 一致(否则 login 在 K8s 上拒启)"

    $prodFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $prodRun.Output -Name "go-svc-login-config")
    Assert-True -Condition (-not $prodFlat.Scalars.Contains('data.login.yaml.Mode')) -Because "prod 档 login ConfigMap 不得写 Mode(默认 pro 才会对占位密钥 fail-closed)"
}

# ─────────────────────────────────────────────────────────────────
# 3b. 削峰(G4b:docs/handoff/launch-readiness-20261013/spec_G4b.md §2、§6):gateway Bucket4j 限流 / login 登录排队
# ─────────────────────────────────────────────────────────────────
# k8s_deploy.ps1 按 -ReleaseProfile 生成:dev 两项都关(产物与 G4b 之前逐字相同),staging / prod 两项都开;阈值只有一份
# ($script:PeakShavingLaunchTier,全部待压测确认)。-GatewayRateLimit / -LoginQueue 取 auto|on|off(auto = 跟随档位),
# off 只用于止血与基线压测。复用上面的 $devOut(dev 缺省)与 $prodRun(prod 缺省),另起 11 个 DryRun 子进程:下面 6 个,
# 用例 10 一个、用例 11 两个、用例 19 / 20 各一个在用例里起。部署子进程一律带 -DryRun($BaseArgs / $ProdArgs 已带),不碰任何集群。
# 2026-10-15 起排队还要过合入门槛(k8s_deploy.ps1 的 $script:LoginQueueMergeGateCleared):未清时任何档位都不开排队。下面断「排队开」
# 的 DryRun 经 $ProdEnv 或显式给的 MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED=1(只在 -DryRun 生效的测试接缝)视为门槛已清;门槛本身见用例 19–22。
# 计划行与警告是 Show-PeakShavingPlan 打的英文原文;警告只断开头不含空格的「[PeakShaving]Xxx=yyy」一段
# (Write-Warning 可能在空格处折行,同下文 MySQL 连接总账、gateway 管理面口令两节的说明)。
$PeakNodeImage = 'ghcr.io/luyuancpp/mmorpg-node:0123456789ab'
$peakStagingRun = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-ReleaseProfile', 'staging', '-SkipPreflight', '-NodeImage', $PeakNodeImage)) -Env $ProdEnv
# 两项都显式关,排队密钥置空:关排队时不该要求它。拷贝用 @{} + 再按键赋值(+ 合并同名键会抛异常)。
$peakProdOffEnv = @{} + $ProdEnv
$peakProdOffEnv['MMORPG_LOGIN_QUEUE_TOKEN_SECRET'] = ''
# 显式 off 不依赖排队开,不带合入门槛的测试接缝:顺带覆盖「门槛未清 + 显式 off」仍打原来的 LoginQueue=off 警告(用例 9)。
$peakProdOffEnv['MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED'] = ''
$peakProdOffRun = Invoke-DeployDryRun -Arguments ($ProdArgs + @('-GatewayRateLimit', 'off', '-LoginQueue', 'off')) -Env $peakProdOffEnv
# dev 档显式开两项(联调用法)。两个变量显式置空,结论不受跑测机器上残留的同名环境变量影响。
# -LoginQueue on 在门槛未清时直接拒绝,所以带上只在 -DryRun 生效的测试接缝(见 $ProdEnv 的说明)。
$peakDevOnRun = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-GatewayRateLimit', 'on', '-LoginQueue', 'on')) -Env @{ MMORPG_LOGIN_QUEUE_TOKEN_SECRET = ''; MMORPG_REDIS_PASSWORD = ''; MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED = '1' }
# 多 zone:zones.sample.json 的全部 zone。-ZonesConfigPath 给绝对路径(子进程的工作目录不保证是仓库根);
# -SkipInfra 不装配 battle 全局池(否则还要 battle 密钥),-SkipJavaSvc:本次只看 login。
$PeakAllUpArgs = @(
    '-Command', 'all-up', '-DryRun', '-SkipInfra', '-SkipJavaSvc',
    '-ZonesConfigPath', (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.sample.json'),
    '-GoSvcRegistry', 'registry.invalid/test',
    '-ReleaseProfile', 'prod', '-SkipPreflight', '-NodeImage', $PeakNodeImage
)
$peakAllUpRun = Invoke-DeployDryRun -Arguments $PeakAllUpArgs -Env $ProdEnv
# 只换 Redis 口令(gateway 削峰指纹的输入之一);-SkipGoSvc:本次只看 gateway。
$PeakRotatedRedisPassword = 'contract-test-redis-pw-rotated-02'
$peakGwRedisRotEnv = @{} + $ProdEnv
$peakGwRedisRotEnv['MMORPG_REDIS_PASSWORD'] = $PeakRotatedRedisPassword
$peakGwRedisRotRun = Invoke-DeployDryRun -Arguments ($ProdArgs + @('-SkipGoSvc')) -Env $peakGwRedisRotEnv
# 只加可信代理网段(gateway 削峰指纹的另一份输入:rate-limit 段文本),密钥与 $prodRun 完全相同;-SkipGoSvc:本次只看 gateway。
# 没有 Ingress 时给可信代理不会被入口预检拒绝(lib/k8s_client_entry.ps1 只在给了 -GatewayIngressHost 却没给它时报错)。
$PeakTrustedProxies = '10.0.0.0/8'
$peakGwProxyRun = Invoke-DeployDryRun -Arguments ($ProdArgs + @('-SkipGoSvc', '-GatewayTrustedProxies', $PeakTrustedProxies)) -Env $ProdEnv

# 拍平后的 manifest 里某个路径本身及其全部子键(序数比较;'Queue' 不会误中 'QueueXxx')。
function Get-FlatSubtreeKeys {
    param(
        [Parameter(Mandatory = $true)]$Flat,
        [Parameter(Mandatory = $true)][string]$Path
    )
    return @($Flat.Scalars.Keys | ForEach-Object { [string]$_ } | Where-Object {
        $_ -ceq $Path -or $_.StartsWith("$Path.", [System.StringComparison]::Ordinal) -or $_.StartsWith("$Path[", [System.StringComparison]::Ordinal)
    })
}

# Pod 模板上的削峰指纹注解(mmorpg.io/peak-shaving-hash)。gateway:紧跟在口令指纹那一行之后(Add-PodTemplateAnnotation 把 -Key
# 恒排在 annotations: 的下一行,下文 Get-GatewayAdminApiKeyHash 依赖这个位置),顺带取出口令指纹;login:annotations: 的下一行。
# Select-ManifestByName 已把块内换行统一成 LF,所以按 \n 锚定(整段原始输出在 Windows 上是 CRLF)。
# 缺失 / 多于一处 / 位置不对 / 不是加双引号的 12 位小写十六进制,一律 throw,不返回半截结果。
function Get-PeakShavingAnnotation {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Output,
        [Parameter(Mandatory = $true)][ValidateSet('gateway', 'login')][string]$Workload,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $block = Select-ManifestByName -Output $Output -Name $Workload
    if ($null -eq $block) { throw "${Label}:DryRun 输出里没有 $Workload 的 manifest" }
    $count = [regex]::Matches($block, 'mmorpg\.io/peak-shaving-hash').Count
    if ($count -ne 1) { throw "${Label}:$Workload manifest 里削峰指纹注解应恰好 1 处,实际 $count 处" }
    $pattern = 'annotations:[ ]*\n[ ]+mmorpg\.io/peak-shaving-hash: "(?<peak>[0-9a-f]{12})"'
    if ($Workload -eq 'gateway') {
        $pattern = 'annotations:[ ]*\n[ ]+mmorpg\.io/gateway-admin-api-key-hash: "(?<admin>[0-9a-f]{12})"[ ]*\n[ ]+mmorpg\.io/peak-shaving-hash: "(?<peak>[0-9a-f]{12})"'
    }
    $m = [regex]::Match($block, $pattern)
    if (-not $m.Success) { throw "${Label}:$Workload 的削峰指纹必须在 Pod 模板 annotations 下(gateway 排在口令指纹之后),值为加双引号的 12 位小写十六进制" }
    return [pscustomobject]@{ Peak = $m.Groups['peak'].Value; AdminKey = $m.Groups['admin'].Value }
}

# 从 manifest 块里取出「<SectionKey>:」那一行连同其下缩进更深的所有行(去掉行尾空白与空行),用于逐字比对两次运行的同一段配置。
# 该键必须恰好出现一处且下面至少有一行,否则 throw(fail-closed:两段都是空的也「相等」,不能算通过)。
function Get-ManifestSectionText {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][AllowNull()][string]$Block,
        [Parameter(Mandatory = $true)][string]$SectionKey,
        [Parameter(Mandatory = $true)][string]$Label
    )
    if ([string]::IsNullOrWhiteSpace($Block)) { throw "${Label}:manifest 块为空" }
    $lines = @($Block -split "`r?`n" | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ -ne '' })
    $keyPattern = '^[ ]*' + [regex]::Escape($SectionKey) + ':$'
    $starts = @(for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -cmatch $keyPattern) { $i } })
    if ($starts.Count -ne 1) { throw "${Label}:「${SectionKey}:」应恰好 1 处,实际 $($starts.Count) 处" }
    $keyIndent = $lines[$starts[0]].Length - $lines[$starts[0]].TrimStart(' ').Length
    $section = New-Object System.Collections.Generic.List[string]
    $section.Add($lines[$starts[0]])
    for ($i = $starts[0] + 1; $i -lt $lines.Count; $i++) {
        if (($lines[$i].Length - $lines[$i].TrimStart(' ').Length) -le $keyIndent) { break }
        $section.Add($lines[$i])
    }
    if ($section.Count -lt 2) { throw "${Label}:「${SectionKey}:」下面没有任何子行" }
    return ($section -join "`n")
}

# 常量表自检(用例 16):按 AST 从 k8s_deploy.ps1 抽出下面三个函数直接调用(与上面 deadline 守卫、下文 Kafka 拓扑同一做法)。
#   - Get-GatewayRateLimitSettings 只读 $script:GatewayRateLimitOn 与 $script:PeakShavingLaunchTier;
#   - New-LoginQueueYaml 读 $script:LoginQueueOn、$script:PeakShavingLaunchTier,并经 Get-AuthoritativeScalar 按 $RepoRoot 读
#     go/login/etc/login.yaml(Get-YamlScalar 来自 deploy_capture.ps1 dot-source 的 release_common.ps1)。
# 脚本级变量由用例在本脚本作用域里设好、用完即删;$RepoRoot 由用例自己设(下文 MySQL 节同样抽 Get-AuthoritativeScalar,重设无害)。
$peakFunctionNames = @('Get-GatewayRateLimitSettings', 'New-LoginQueueYaml', 'Get-AuthoritativeScalar')
foreach ($peakFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($peakFunctionNames -contains $peakFnAst.Name) {
        Set-Item -Path "Function:script:$($peakFnAst.Name)" -Value $peakFnAst.Body.GetScriptBlock()
    }
}
foreach ($peakFnName in $peakFunctionNames) {
    if (-not (Test-Path "Function:$peakFnName")) { throw "k8s_deploy.ps1 缺少函数 $peakFnName(削峰阈值 / 排队段的生成与自检被删了?后续断言无意义)" }
}

Test-Case '削峰 dev 档缺省关:gateway 不写 gate.rate-limit.enabled,login 不写 Queue / Secrets.QueueToken,两者无 peak-shaving 注解(产物与 G4b 前相同)' {
    # dev 档(auto)两项都关:落回 jar 内 / login.yaml 的 enabled: false,本地与 kind 联调不受削峰影响。
    # 基线 DryRun 没给 -GatewayTrustedProxies,所以 gateway 的 application.yaml 里连 rate-limit 键都不该有。
    $gatewayFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'java-svc-gateway-config')
    $rateLimitKeys = @(Get-FlatSubtreeKeys -Flat $gatewayFlat -Path 'data.application.yaml.gate.rate-limit')
    Assert-True -Condition ($rateLimitKeys.Count -eq 0) -Because "dev 档 gateway ConfigMap 不得写 gate.rate-limit(实际出现:$($rateLimitKeys -join ', '))"
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'go-svc-login-config')
    $queueKeys = @(@(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Queue') + @(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Secrets.QueueToken'))
    Assert-True -Condition ($queueKeys.Count -eq 0) -Because "dev 档 login ConfigMap 不得写 Queue / Secrets.QueueToken(实际出现:$($queueKeys -join ', '))"
    Assert-NotMatch -Text $devOut -Pattern 'peak-shaving-hash' -Because 'dev 档 gateway / login 的 Pod 模板都不该带削峰指纹(关 = 不写注解,模板与 G4b 之前相同)'
    Assert-Match -Text $devOut -Pattern 'PeakShaving: gateway_rate_limit=off\(-GatewayRateLimit auto\) login_queue=off\(-LoginQueue auto\) profile=dev ' -Because '写路径入口要打出两个开关的解析结果:dev 档 auto = 关'
}

Test-Case '削峰 staging / prod 档开:gateway 阈值 == 开服档起步值,rate-limit: 只有一个' {
    # 负责人 2026-10-13 拍板的开服档起步值:zone 2000/s、突发 5000;IP 5/s、突发 20;账号冷却 5000ms;排队容量 5000/zone。
    # 全部待压测确认(L-14)。这张表就是被评审的那个决定:改 k8s_deploy.ps1 的 $script:PeakShavingLaunchTier 要同步改这里。
    $expected = [ordered]@{
        'enabled'             = 'true'
        'zone-default-rps'    = '2000'
        'zone-default-burst'  = '5000'
        'ip-rps'              = '5'
        'ip-burst'            = '20'
        'account-cooldown-ms' = '5000'
    }
    foreach ($case in @(@{ Label = 'staging'; Run = $peakStagingRun }, @{ Label = 'prod'; Run = $prodRun })) {
        Assert-Equal -Expected 0 -Actual $case.Run.ExitCode -Because "$($case.Label) 档 DryRun 应当成功。输出: $($case.Run.Output)"
        $block = Select-ManifestByName -Output $case.Run.Output -Name 'java-svc-gateway-config'
        $flat = ConvertTo-FlatManifest -Block $block
        foreach ($key in $expected.Keys) {
            Assert-Equal -Expected $expected[$key] -Actual (Get-FlatValue -Flat $flat -KeyPath "data.application.yaml.gate.rate-limit.$key") -Because "$($case.Label) 档 gate.rate-limit.$key 必须是开服档起步值"
        }
        # 阈值与 trusted-proxies 共用一个 rate-limit: 键:另起一个就是重复键,Spring 的 SnakeYAML 直接拒启。
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, '(?m)^[ ]+rate-limit:[ ]*\r?$').Count) -Because "$($case.Label) 档 java-svc-gateway-config 里 rate-limit: 只能有一个"
        # 与 release_preflight.ps1 F 节同一条规则(rps ≥ 1、burst ≥ rps),这里核的是集群里真正生效的那份值。
        foreach ($pair in @(@('zone-default-rps', 'zone-default-burst'), @('ip-rps', 'ip-burst'))) {
            $rps = [long](Get-FlatValue -Flat $flat -KeyPath "data.application.yaml.gate.rate-limit.$($pair[0])")
            $burst = [long](Get-FlatValue -Flat $flat -KeyPath "data.application.yaml.gate.rate-limit.$($pair[1])")
            Assert-True -Condition ($rps -ge 1 -and $burst -ge $rps) -Because "$($case.Label) 档 $($pair[0])=$rps / $($pair[1])=$burst 必须满足 rps ≥ 1 且 burst ≥ rps"
        }
        $planPattern = 'PeakShaving: gateway_rate_limit=on\(-GatewayRateLimit auto\) login_queue=on\(-LoginQueue auto\) profile=' + $case.Label +
            ' initial values \(pending stress-test validation\) zone=2000/s burst=5000 ip=5/s burst=20 cooldown=5000ms queue_capacity=5000/zone'
        Assert-Match -Text $case.Run.Output -Pattern $planPattern -Because "$($case.Label) 档 auto = 开,写路径入口要打出开关解析结果与起步值"
    }
}

Test-Case '削峰 staging / prod 档开:login Queue 段除三项外 == go/login/etc/login.yaml,锁键带本 zone,ZoneCapacityOverride 只有本 zone' {
    $loginYaml = 'go/login/etc/login.yaml'
    # Get-EtcValue 去掉引号后的基名。G1b 之后 login 把配置值当前缀、自己再拼一次 :zone:<id>;这里显式带 zone 防的是
    # rollout undo 退回 G1b 之前的镜像(见 k8s_deploy.ps1 New-LoginQueueYaml)。
    $lockKeyBase = Get-EtcValue -RelativePath $loginYaml -KeyPath 'Queue.DispatcherLockKey'
    foreach ($case in @(@{ Label = 'staging'; Run = $peakStagingRun }, @{ Label = 'prod'; Run = $prodRun })) {
        $block = Select-ManifestByName -Output $case.Run.Output -Name 'go-svc-login-config'
        $flat = ConvertTo-FlatManifest -Block $block
        Assert-Equal -Expected 'true' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Queue.Enabled') -Because "$($case.Label) 档 login 排队必须打开"
        foreach ($key in @('DispatchInterval', 'AdmitTTL', 'QueueEntryTTL', 'SoftCapMultiplier', 'DefaultRetryAfterMs', 'DispatcherLockTTL')) {
            Assert-Equal -Expected (Get-EtcValue -RelativePath $loginYaml -KeyPath "Queue.$key") -Actual (Get-FlatValue -Flat $flat -KeyPath "data.login.yaml.Queue.$key") -Because "$($case.Label) 档 Queue.$key 必须镜像 $loginYaml(生成器不得自带常数)"
        }
        $lockKey = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Queue.DispatcherLockKey'
        Assert-True -Condition ($lockKey -ceq "${lockKeyBase}:zone:101") -Because "$($case.Label) 档锁键必须是 <login.yaml 基名>:zone:<本 zone>(期望 '${lockKeyBase}:zone:101',实际 '$lockKey')"
        $capacityKeys = @(Get-FlatSubtreeKeys -Flat $flat -Path 'data.login.yaml.Queue.ZoneCapacityOverride' | Where-Object { $_ -cne 'data.login.yaml.Queue.ZoneCapacityOverride' })
        Assert-Equal -Expected 'data.login.yaml.Queue.ZoneCapacityOverride.101' -Actual ($capacityKeys -join ',') -Because "$($case.Label) 档 ZoneCapacityOverride 只写本 zone(每个 zone 的 login 只出队本 zone)"
        Assert-Equal -Expected '5000' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Queue.ZoneCapacityOverride.101') -Because "$($case.Label) 档本 zone 的排队容量是开服档起步值 5000(同时在线上限,不是速率)"
        # ConfigMap 把 login.yaml 整体缩进 4 格嵌在 login.yaml: | 下;Queue 与 Secrets 都只能各有一个顶层键
        # (QueueToken 另起一个 Secrets: 就是重复键,后者覆盖前者,InternalAuth 会丢)。
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, '(?m)^ {4}Queue:[ ]*\r?$').Count) -Because "$($case.Label) 档 login.yaml 里顶层 Queue: 只能有一个"
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, '(?m)^ {4}Secrets:[ ]*\r?$').Count) -Because "$($case.Label) 档 QueueToken 必须并进已有的 Secrets: 段"
    }
}

Test-Case '削峰写出的键名与服务自己的配置同名(拼错会被静默忽略)' {
    # Spring 对 gate.rate-limit 下不认识的键、go-zero 对 Queue 下不认识的键都不报错,只是不生效 —— 只能在这里逐键核对。
    $gatewayYaml = 'java/gateway_node/src/main/resources/application.yaml'
    $gatewayFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $prodRun.Output -Name 'java-svc-gateway-config')
    $rateLimitPrefix = 'data.application.yaml.gate.rate-limit.'
    $rateLimitKeys = @(Get-FlatSubtreeKeys -Flat $gatewayFlat -Path 'data.application.yaml.gate.rate-limit' |
        Where-Object { $_.StartsWith($rateLimitPrefix, [System.StringComparison]::Ordinal) } | ForEach-Object { $_.Substring($rateLimitPrefix.Length) })
    Assert-Equal -Expected 6 -Actual $rateLimitKeys.Count -Because "prod 基线(未给 -GatewayTrustedProxies)应恰好写出 6 个阈值键(实际:$($rateLimitKeys -join ', '))"
    foreach ($key in $rateLimitKeys) {
        $found = Get-YamlScalar -Path (Join-Path (Get-RepoRoot) $gatewayYaml) -KeyPath "gate.rate-limit.$key"
        Assert-True -Condition $found.Found -Because "gate.rate-limit.$key 在 $gatewayYaml 里查不到:键名拼错,Spring 静默忽略,限流不按写出的值生效"
    }

    $loginYaml = 'go/login/etc/login.yaml'
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $prodRun.Output -Name 'go-svc-login-config')
    $queuePrefix = 'data.login.yaml.Queue.'
    $queueKeys = @(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Queue' |
        Where-Object { $_.StartsWith($queuePrefix, [System.StringComparison]::Ordinal) } | ForEach-Object { $_.Substring($queuePrefix.Length) } |
        Where-Object { -not $_.Contains('.') })
    Assert-True -Condition ($queueKeys -ccontains 'ZoneCapacityOverride') -Because "prod 基线应当写出 Queue.ZoneCapacityOverride(实际 Queue 下的键:$($queueKeys -join ', '))"
    $configGo = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'go/login/internal/config/config.go') -Raw
    $queueConf = [regex]::Match($configGo, '(?s)type QueueConf struct \{(?<body>.*?)\r?\n\}')
    Assert-True -Condition $queueConf.Success -Because 'go/login/internal/config/config.go 里应当有 type QueueConf struct(本断言的前提)'
    foreach ($key in $queueKeys) {
        # go-zero 按 json tag 绑定:每个写出的键都必须是 QueueConf 某个字段的 tag 名(区分大小写)。
        Assert-True -Condition ($queueConf.Groups['body'].Value -cmatch ('json:"' + [regex]::Escape($key) + '[,"]')) -Because "Queue.$key 不是 go/login/internal/config/config.go QueueConf 的字段:拼错的键 login 静默忽略"
        # login.yaml 里 ZoneCapacityOverride 只有注释示例,其余每个键(Enabled、6 个镜像键、DispatcherLockKey)都必须读得到。
        if ($key -cne 'ZoneCapacityOverride') {
            $found = Get-YamlScalar -Path (Join-Path (Get-RepoRoot) $loginYaml) -KeyPath "Queue.$key"
            Assert-True -Condition $found.Found -Because "Queue.$key 在 $loginYaml 里查不到:生成器写了服务自己的配置里没有的键"
        }
    }
    Assert-True -Condition ($queueConf.Groups['body'].Value.Contains('json:"ZoneCapacityOverride,optional"')) -Because 'ZoneCapacityOverride 在 login.yaml 里只有注释示例,改核 Go 结构体的 json tag(optional:不配即走回落公式)'
}

Test-Case '「不写即关」的前提:jar 内 gate.rate-limit.enabled 与 login.yaml Queue.Enabled 仍为 false,两处 Go/Java 缺省也是 false' {
    # dev 档与 off 靠「不写」关闭削峰:ConfigMap 不写这两段,就落回 jar 内 application.yaml / login.yaml,再落回类型缺省值。
    # 这四处任何一处翻成 true,dev 档就会在没人注意的情况下开着削峰跑。
    Assert-Equal -Expected 'false' -Actual (Get-EtcValue -RelativePath 'java/gateway_node/src/main/resources/application.yaml' -KeyPath 'gate.rate-limit.enabled') -Because 'jar 内 gate.rate-limit.enabled 必须保持 false'
    Assert-Equal -Expected 'false' -Actual (Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath 'Queue.Enabled') -Because 'go/login/etc/login.yaml 的 Queue.Enabled 必须保持 false'
    $propsText = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'java/gateway_node/src/main/java/com/game/gateway/ratelimit/RateLimitProperties.java') -Raw
    # 顶层字段缩进 4 格;嵌套类 WaveSchedule 里的同名字段缩进 8 格,不能拿它凑数。
    Assert-True -Condition ($propsText -cmatch '(?m)^ {4}private boolean enabled = false;') -Because 'RateLimitProperties 的总开关缺省必须是 false'
    $configGo = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'go/login/internal/config/config.go') -Raw
    $queueConf = [regex]::Match($configGo, '(?s)type QueueConf struct \{(?<body>.*?)\r?\n\}')
    Assert-True -Condition $queueConf.Success -Because 'go/login/internal/config/config.go 里应当有 type QueueConf struct(本断言的前提)'
    Assert-True -Condition ($queueConf.Groups['body'].Value -cmatch ('(?m)^\s*Enabled\s+bool\s+' + [regex]::Escape('`json:"Enabled,default=false"`'))) -Because 'QueueConf.Enabled 的 go-zero 缺省必须是 false'
}

Test-Case 'staging / prod 的 gateway 带 spring.data.redis.password 且 == MMORPG_REDIS_PASSWORD;dev 无 Redis 口令时不写' {
    # Bucket4j 的 Lettuce 客户端只从 spring.data.redis.password 取口令;release 档 Redis 带 --requirepass,不写口令 = 限流的 Redis
    # 调用过不了认证(见 k8s_deploy.ps1 New-JavaSvcConfigMapYaml)。只看口令是否非空,与限流开关无关。测试不回显口令值。
    foreach ($case in @(@{ Label = 'staging'; Run = $peakStagingRun }, @{ Label = 'prod'; Run = $prodRun })) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $case.Run.Output -Name 'java-svc-gateway-config')
        $password = Get-FlatValue -Flat $flat -KeyPath 'data.application.yaml.spring.data.redis.password'
        Assert-True -Condition ($password -ceq $ProdEnv.MMORPG_REDIS_PASSWORD) -Because "$($case.Label) 档 gateway 的 Redis 口令必须来自 MMORPG_REDIS_PASSWORD(与 go-svc ConfigMap 同一个值)"
    }
    Assert-Equal -Expected 0 -Actual $peakDevOnRun.ExitCode -Because "dev 档显式开削峰的 DryRun 应当成功。输出: $($peakDevOnRun.Output)"
    $devFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakDevOnRun.Output -Name 'java-svc-gateway-config')
    Assert-True -Condition (-not $devFlat.Scalars.Contains('data.application.yaml.spring.data.redis.password')) -Because 'dev 档没有 Redis 口令(MMORPG_REDIS_PASSWORD 为空)时不写 password 键,dev 产物逐字不变'
}

Test-Case 'login Secrets.QueueToken.Value == MMORPG_LOGIN_QUEUE_TOKEN_SECRET,InternalAuth 不受影响' {
    foreach ($case in @(@{ Label = 'staging'; Run = $peakStagingRun }, @{ Label = 'prod'; Run = $prodRun })) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $case.Run.Output -Name 'go-svc-login-config')
        $queueToken = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Secrets.QueueToken.Value'
        Assert-True -Condition ($queueToken -ceq $ProdEnv.MMORPG_LOGIN_QUEUE_TOKEN_SECRET) -Because "$($case.Label) 档 Secrets.QueueToken 必须来自 MMORPG_LOGIN_QUEUE_TOKEN_SECRET(生产模式开排队缺它 login 拒启)"
        $internalAuth = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Secrets.InternalAuth.Value'
        Assert-True -Condition ($internalAuth -ceq $ProdEnv.MMORPG_INTERNAL_AUTH_SECRET) -Because "$($case.Label) 档 Secrets.InternalAuth 仍须是 MMORPG_INTERNAL_AUTH_SECRET:QueueToken 并进同一个 Secrets: 段,不能把它挤掉"
    }
}

Test-Case 'peak-shaving 指纹:gateway 在口令指纹之后多一条、login 一条;staging 与 prod 同输入同指纹;off 时消失' {
    # ConfigMap 只在进程启动时读:只改 ConfigMap 时 Pod 模板不变、apply 是 no-op。指纹进模板,开关 / 阈值 / 排队密钥一变就滚动。
    $stagingGateway = Get-PeakShavingAnnotation -Output $peakStagingRun.Output -Workload 'gateway' -Label 'staging'
    $prodGateway = Get-PeakShavingAnnotation -Output $prodRun.Output -Workload 'gateway' -Label 'prod'
    $stagingLogin = Get-PeakShavingAnnotation -Output $peakStagingRun.Output -Workload 'login' -Label 'staging'
    $prodLogin = Get-PeakShavingAnnotation -Output $prodRun.Output -Workload 'login' -Label 'prod'
    # 指纹只由「写进 ConfigMap 的那段配置 + 相关密钥」决定:两次独立子进程、不同档位、同一输入,指纹必须相同,否则每次重跑都无故滚动。
    Assert-Equal -Expected $prodGateway.Peak -Actual $stagingGateway.Peak -Because 'gateway:staging 与 prod 的 rate-limit 段与 Redis 口令相同,指纹必须相同'
    Assert-Equal -Expected $prodLogin.Peak -Actual $stagingLogin.Peak -Because 'login:staging 与 prod 的 Queue 段与排队密钥相同,指纹必须相同'
    Assert-Equal -Expected 0 -Actual $peakProdOffRun.ExitCode -Because "下面断「off 时没有指纹」的前提是这次 DryRun 成功。输出: $($peakProdOffRun.Output)"
    foreach ($case in @(@{ Label = '-GatewayRateLimit off -LoginQueue off(prod)'; Output = $peakProdOffRun.Output }, @{ Label = 'dev 缺省'; Output = $devOut })) {
        Assert-NotMatch -Text $case.Output -Pattern 'peak-shaving-hash' -Because "$($case.Label):关 = 不写指纹注解(开 → 关时模板变化,同样自动滚动)"
    }
    # 排队密钥是 login 指纹的输入:dev 档未注入(空串)与 prod 注入的值不同,Queue 段相同,指纹必须不同(轮换排队密钥自动滚动 login)。
    $devOnLogin = Get-PeakShavingAnnotation -Output $peakDevOnRun.Output -Workload 'login' -Label 'dev -LoginQueue on'
    Assert-True -Condition ($devOnLogin.Peak -ne $prodLogin.Peak) -Because "排队密钥不同,login 指纹必须不同(两者都是 $($prodLogin.Peak))"
}

Test-Case '-GatewayRateLimit off -LoginQueue off(prod):两段不写、不要求排队密钥、打出关闭警告' {
    Assert-Equal -Expected 0 -Actual $peakProdOffRun.ExitCode -Because "止血 / 基线压测路径:排队密钥置空也必须能部署。输出: $($peakProdOffRun.Output)"
    $gatewayFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakProdOffRun.Output -Name 'java-svc-gateway-config')
    Assert-True -Condition (-not $gatewayFlat.Scalars.Contains('data.application.yaml.gate.rate-limit.enabled')) -Because 'off 不写 gate.rate-limit.enabled,落回 jar 内的 false'
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakProdOffRun.Output -Name 'go-svc-login-config')
    $queueKeys = @(@(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Queue') + @(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Secrets.QueueToken'))
    Assert-True -Condition ($queueKeys.Count -eq 0) -Because "off 不写 Queue / Secrets.QueueToken(实际出现:$($queueKeys -join ', '))"
    Assert-Match -Text $peakProdOffRun.Output -Pattern 'PeakShaving: gateway_rate_limit=off\(-GatewayRateLimit off\) login_queue=off\(-LoginQueue off\) profile=prod ' -Because '计划行要打出显式 off 的解析结果'
    Assert-Match -Text $peakProdOffRun.Output -Pattern '\[PeakShaving\]GatewayRateLimit=off' -Because '非 dev 档关限流必须告警(只用于止血 / 基线压测,参数不粘滞)'
    Assert-Match -Text $peakProdOffRun.Output -Pattern '\[PeakShaving\]LoginQueue=off' -Because '非 dev 档关排队必须告警'
    # $peakProdOffRun 不带合入门槛的测试接缝(门槛未清):显式 off 的结论与门槛无关,仍打上面那条,不打门槛警告。
    Assert-NotMatch -Text $peakProdOffRun.Output -Pattern '\[PeakShaving\]LoginQueueGate=closed' -Because '显式 -LoginQueue off 时维持原来的 LoginQueue=off 警告,不改打合入门槛警告'
    Assert-NotMatch -Text $peakProdOffRun.Output -Pattern '\[PeakShaving\]GatewayTrustedProxies=unset' -Because '限流关着时不打「未配置可信代理」警告'
    # 对照:prod 缺省(两项都开、未给 -GatewayTrustedProxies)不打两条关闭警告,只打可信代理那一条 —— 否则区分不出「关了才告警」。
    Assert-NotMatch -Text $prodRun.Output -Pattern '\[PeakShaving\](GatewayRateLimit|LoginQueue)=off' -Because '对照:两项都开时不该打关闭警告'
    Assert-NotMatch -Text $prodRun.Output -Pattern '\[PeakShaving\]LoginQueueGate=closed' -Because '对照:$prodRun 带测试接缝(视为门槛已清),排队按档位开,不该打合入门槛警告'
    Assert-Match -Text $prodRun.Output -Pattern '\[PeakShaving\]GatewayTrustedProxies=unset' -Because '对照:限流开、部署 gateway 却没给 -GatewayTrustedProxies 时要告警(IP 桶按 socket 对端分)'
}

Test-Case '负向:prod 缺 MMORPG_LOGIN_QUEUE_TOKEN_SECRET 在任何写操作之前拒绝并点名变量' {
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_LOGIN_QUEUE_TOKEN_SECRET'] = ''
    $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2
    Assert-True -Condition ($run.ExitCode -ne 0) -Because "prod 开排队、部署 login 却缺排队密钥必须阻断:login 生产模式拒启,部署出来只会 CrashLoopBackOff。输出: $($run.Output)"
    # 其余生产密钥都已给齐($ProdEnv 基线能出产物),拒绝只能来自这一把;断 Resolve-InjectedSecret「未设置」分支的原文开头。
    Assert-Match -Text $run.Output -Pattern 'ReleaseProfile=prod 要求从环境变量 MMORPG_LOGIN_QUEUE_TOKEN_SECRET' -Because '错误必须点名缺哪个环境变量,且说清是注入缺失而不是别的失败'
    Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl [^\r\n]*\b(apply|create|delete|patch|replace)\b' -Because '密钥解析(Initialize-InjectedSecrets)先于任何写操作,不留半截部署'
    # 计划行排在密钥解析之前(k8s_deploy.ps1 写路径入口):被拒时输出里已有 login_queue=on 可对照。
    Assert-Match -Text $run.Output -Pattern 'login_queue=on\(-LoginQueue auto\)' -Because '被拒时操作者要能看出排队是按档位(auto)开的'
}

Test-Case '负向:排队密钥与 GATE / INTERNAL_AUTH 相同时拒绝,只点变量名不回显值' {
    foreach ($reused in @(
        @{ EnvName = 'MMORPG_GATE_TOKEN_SECRET'; SecretValue = $ProdEnv.MMORPG_GATE_TOKEN_SECRET }
        @{ EnvName = 'MMORPG_INTERNAL_AUTH_SECRET'; SecretValue = $ProdEnv.MMORPG_INTERNAL_AUTH_SECRET }
    )) {
        $env2 = @{} + $ProdEnv
        $env2['MMORPG_LOGIN_QUEUE_TOKEN_SECRET'] = $reused.SecretValue
        $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2
        Assert-True -Condition ($run.ExitCode -ne 0) -Because "排队密钥复用 $($reused.EnvName) 必须阻断(login 拒绝跨用途复用主密钥)。输出: $($run.Output)"
        Assert-Match -Text $run.Output -Pattern "必须与 $($reused.EnvName) 不同" -Because "错误必须点名与哪一把重复($($reused.EnvName))"
        Assert-NotMatch -Text $run.Output -Pattern ([regex]::Escape($reused.SecretValue)) -Because "$($reused.EnvName):拒绝路径只点变量名,不回显密钥值"
        Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl [^\r\n]*\b(apply|create|delete|patch|replace)\b' -Because "$($reused.EnvName):复用检查先于任何写操作"
    }
}

Test-Case 'dev -GatewayRateLimit on -LoginQueue on:两段写出,不写 Secrets.QueueToken,不要求注入,Mode 仍为 dev' {
    Assert-Equal -Expected 0 -Actual $peakDevOnRun.ExitCode -Because "dev 档联调:排队密钥未注入也不阻断。输出: $($peakDevOnRun.Output)"
    $gatewayFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakDevOnRun.Output -Name 'java-svc-gateway-config')
    Assert-Equal -Expected 'true' -Actual (Get-FlatValue -Flat $gatewayFlat -KeyPath 'data.application.yaml.gate.rate-limit.enabled') -Because '显式 on 在任何档位都写限流段'
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakDevOnRun.Output -Name 'go-svc-login-config')
    Assert-Equal -Expected 'true' -Actual (Get-FlatValue -Flat $loginFlat -KeyPath 'data.login.yaml.Queue.Enabled') -Because '显式 on 在任何档位都写排队段'
    $queueTokenKeys = @(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Secrets.QueueToken')
    Assert-True -Condition ($queueTokenKeys.Count -eq 0) -Because "dev 档排队密钥为空时不写 Secrets.QueueToken(login 宽松档回落 GateToken 密钥并告警)(实际出现:$($queueTokenKeys -join ', '))"
    # 回落只在宽松档(Mode dev / test)成立:dev 档照写 Mode,不写 QueueToken 的 login 才起得来。
    Assert-Equal -Expected (Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath 'Mode') -Actual (Get-FlatValue -Flat $loginFlat -KeyPath 'data.login.yaml.Mode') -Because 'dev 档 login ConfigMap 的 Mode 必须仍与 go/login/etc/login.yaml 一致'
    Assert-Match -Text $peakDevOnRun.Output -Pattern 'PeakShaving: gateway_rate_limit=on\(-GatewayRateLimit on\) login_queue=on\(-LoginQueue on\) profile=dev ' -Because '计划行要打出显式 on 的解析结果'
}

Test-Case 'all-up 多 zone(prod):每个 zone 的 login 锁键与 ZoneCapacityOverride 只指向本 zone,两把锁键互不相同,login 削峰指纹也互不相同' {
    Assert-True -Condition ($PeakAllUpArgs -contains '-DryRun') -Because '夹具前提:all-up 必须是 DryRun(不带它会对本机当前 kubectl context 真执行 apply)'
    $expectedZoneIds = @((Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.sample.json') -Raw | ConvertFrom-Json).zones | ForEach-Object { [string]$_.zoneId } | Sort-Object)
    Assert-True -Condition ($expectedZoneIds.Count -ge 2) -Because "夹具前提:deploy/k8s/zones.sample.json 至少有两个 zone(实际 $($expectedZoneIds.Count) 个)"
    Assert-Equal -Expected 0 -Actual $peakAllUpRun.ExitCode -Because "多 zone 的 prod all-up DryRun 应当成功。输出: $($peakAllUpRun.Output)"
    $lockKeyBase = Get-EtcValue -RelativePath 'go/login/etc/login.yaml' -KeyPath 'Queue.DispatcherLockKey'
    $blocks = @(Get-ManifestBlocks -Output $peakAllUpRun.Output | Where-Object { $_ -match '(?m)^\s*name:\s*go-svc-login-config\s*$' })
    Assert-Equal -Expected $expectedZoneIds.Count -Actual $blocks.Count -Because '每个 zone 各有一份 go-svc-login-config'
    $zoneIds = @()
    $lockKeys = @()
    foreach ($block in $blocks) {
        $flat = ConvertTo-FlatManifest -Block $block
        $zoneId = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Node.ZoneId'
        $lockKey = Get-FlatValue -Flat $flat -KeyPath 'data.login.yaml.Queue.DispatcherLockKey'
        Assert-True -Condition ($lockKey -ceq "${lockKeyBase}:zone:$zoneId") -Because "zone $zoneId 的锁键必须带本 zone(期望 '${lockKeyBase}:zone:$zoneId',实际 '$lockKey')"
        $capacityKeys = @(Get-FlatSubtreeKeys -Flat $flat -Path 'data.login.yaml.Queue.ZoneCapacityOverride' | Where-Object { $_ -cne 'data.login.yaml.Queue.ZoneCapacityOverride' })
        Assert-Equal -Expected "data.login.yaml.Queue.ZoneCapacityOverride.$zoneId" -Actual ($capacityKeys -join ',') -Because "zone $zoneId 的 ZoneCapacityOverride 只能有本 zone 一个键"
        $zoneIds += $zoneId
        $lockKeys += $lockKey
    }
    Assert-Equal -Expected ($expectedZoneIds -join ',') -Actual (@($zoneIds | Sort-Object) -join ',') -Because 'login ConfigMap 的 Node.ZoneId 与 zones.sample.json 的 zone 一一对应'
    Assert-Equal -Expected $expectedZoneIds.Count -Actual (@($lockKeys | Sort-Object -Unique).Count) -Because '各 zone 的锁键必须互不相同(同一把锁 = 全集群只有一个 leader,其它 zone 的队列不出队)'

    # login 指纹的输入里有配置文本(Queue 段),不只有排队密钥:各 zone 用同一把密钥,Queue 段只差 zone(锁键与 ZoneCapacityOverride),
    # 指纹必须各不相同。指纹若退化成只含密钥,改排队配置后 login 不会滚动,而用例 8 的「dev on 对 prod」(两边只差密钥)照样绿。
    # 不用 Get-PeakShavingAnnotation:它经 Select-ManifestByName 只取第一个同名块,拿不到第二个 zone 的 login。
    $loginDeployments = @(Get-ManifestBlocks -Output $peakAllUpRun.Output | Where-Object {
        $_ -match '(?m)^\s*kind:\s*Deployment\s*$' -and $_ -match '(?m)^\s*name:\s*login\s*$' -and $_.Contains('mmorpg.io/peak-shaving-hash')
    })
    Assert-Equal -Expected $expectedZoneIds.Count -Actual $loginDeployments.Count -Because '每个 zone 各有一份带削峰指纹的 login Deployment'
    $loginPeaks = @()
    foreach ($block in $loginDeployments) {
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, 'mmorpg\.io/peak-shaving-hash').Count) -Because 'login manifest 里削峰指纹注解应恰好 1 处'
        # Get-ManifestBlocks 已把块内换行统一成 LF,与 Get-PeakShavingAnnotation 同一个锚定写法。
        $peakMatch = [regex]::Match($block, 'annotations:[ ]*\n[ ]+mmorpg\.io/peak-shaving-hash: "(?<peak>[0-9a-f]{12})"')
        Assert-True -Condition $peakMatch.Success -Because 'login 的削峰指纹必须在 Pod 模板 annotations 的下一行,值为加双引号的 12 位小写十六进制'
        $loginPeaks += $peakMatch.Groups['peak'].Value
    }
    Assert-Equal -Expected $expectedZoneIds.Count -Actual (@($loginPeaks | Sort-Object -Unique).Count) -Because "同一把排队密钥下各 zone 的 Queue 段不同,login 指纹必须互不相同(实际:$($loginPeaks -join ', '))"
}

Test-Case 'staging 与 prod 的削峰产物逐字相同(档位只决定开关,不决定阈值)' {
    foreach ($section in @(
        @{ ConfigMap = 'java-svc-gateway-config'; SectionKey = 'rate-limit' }
        @{ ConfigMap = 'go-svc-login-config'; SectionKey = 'Queue' }
    )) {
        $stagingText = Get-ManifestSectionText -Block (Select-ManifestByName -Output $peakStagingRun.Output -Name $section.ConfigMap) -SectionKey $section.SectionKey -Label "staging $($section.ConfigMap)"
        $prodText = Get-ManifestSectionText -Block (Select-ManifestByName -Output $prodRun.Output -Name $section.ConfigMap) -SectionKey $section.SectionKey -Label "prod $($section.ConfigMap)"
        Assert-True -Condition ($stagingText -ceq $prodText) -Because "$($section.ConfigMap) 的 $($section.SectionKey): 段 staging 与 prod 必须逐字相同。staging:`n$stagingText`nprod:`n$prodText"
    }
}

Test-Case 'G4b-2 前置:G1b 与 G4b-G 已合入(DispatcherLockKey 是前缀,锁键按 zone 派生;在途占位是新键 ZSET)' {
    # 生成器给每个 zone 显式写 <基名>:zone:<id>,前提是 login 把配置值当前缀、只出队本 zone(G1b)。login 若改成「一个 leader
    # 覆盖全部 zone」,显式分 zone 键就变成 N 个 leader 同时出队所有 zone、超发准入 —— 这条只检查 G1b 的代码形状还在。
    # 2026-10-15 更正:此前这里与下面都写「钉成机械门禁」,不准确。本例只看 G1b / G4b-G 的代码形状,证明不了合入门槛
    # (G1b 多 zone 用例、G4b-G 由 Codex 跑绿,spec_G4b-G.md §8.4 真实 Redis 冒烟记进 PROGRESS);真正挡住打开排队的是
    # k8s_deploy.ps1 的 $script:LoginQueueMergeGateCleared(用例 19–22)。
    $dispatcherGo = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'go/login/internal/logic/pkg/loginqueue/dispatcher.go') -Raw
    Assert-True -Condition ($dispatcherGo -cmatch 'func ZoneDispatcherLockKey\(') -Because 'loginqueue.ZoneDispatcherLockKey 必须存在(G1b:锁键 = 配置前缀 + 本 zone)'
    Assert-True -Condition ($dispatcherGo.Contains('"%s:zone:%d"')) -Because '锁键派生格式必须是 <前缀>:zone:<id>(生成器显式写的键与排查时要查的键都按它推)'
    $svcGo = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'go/login/internal/svc/servicecontext.go') -Raw
    Assert-True -Condition (-not $svcGo.Contains('activeZones := func')) -Because 'G1b 之前「一个 leader 遍历 activeZones 出队」的实现必须已删除'
    # G4b-G:在途占位改成逐个到期的 admitted:zset:zone:{z}。G4b-2 一打开排队,G4b-G 之前的 SET 版本就把 Q1(快速通道占位被重复
    # 扣减、放一批停最长 60s)与 Q2(取走即撤占位、开区超发数倍容量)带上线 —— 下面两条只检查 G4b-G 的代码形状还在
    # (docs/handoff/launch-readiness-20261013/spec_G4b-G.md §5.5;不是对合入门槛的拦截,见上面 2026-10-15 更正)。
    $queueGo = Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'go/login/internal/logic/pkg/loginqueue/queue.go') -Raw
    Assert-True -Condition ($queueGo.Contains('admitted:zset:zone:%d')) -Because '在途占位键必须是 admitted:zset:zone:<id>(G4b-G:ZSET,成员按分数逐个到期)'
    Assert-True -Condition (-not $queueGo.Contains('"admitted:zone:%d"')) -Because 'G4b-G 之前的 SET 键 "admitted:zone:%d" 必须已不再使用(新版本不读不写旧键)'
}

Test-Case '常量表自检:改坏 PeakShavingLaunchTier 时在任何写操作之前 throw' {
    # release_preflight.ps1 的「rps ≥ 1、burst ≥ rps」只查 jar 内 application.yaml;K8s 上真正生效的是 ConfigMap 里的值,
    # 同一条规则由 Get-GatewayRateLimitSettings 再守一次。每次取一份新的合法表,下面逐项改坏。
    $newTier = {
        [ordered]@{ ZoneDefaultRps = 2000; ZoneDefaultBurst = 5000; IpRps = 5; IpBurst = 20; AccountCooldownMs = 5000; LoginQueueZoneCapacity = 5000 }
    }
    try {
        $script:GatewayRateLimitOn = $true
        foreach ($bad in @(
            @{ Why = 'zone 突发小于速率'; Field = 'ZoneDefaultBurst'; NewValue = 1000; Expected = 'ZoneDefaultRps=2000 / ZoneDefaultBurst=1000 不满足 rps ≥ 1 且 burst ≥ rps' }
            @{ Why = 'IP 速率为 0'; Field = 'IpRps'; NewValue = 0; Expected = 'IpRps=0 / IpBurst=20 不满足 rps ≥ 1 且 burst ≥ rps' }
            @{ Why = '账号冷却为负'; Field = 'AccountCooldownMs'; NewValue = -1; Expected = 'AccountCooldownMs=-1 不能为负' }
        )) {
            $tier = & $newTier
            $tier[$bad.Field] = $bad.NewValue
            $script:PeakShavingLaunchTier = $tier
            $message = ''
            try { Get-GatewayRateLimitSettings | Out-Null } catch { $message = $_.Exception.Message }
            Assert-Match -Text $message -Pattern ([regex]::Escape($bad.Expected)) -Because "$($bad.Why) 必须 throw 并点名常量与值,不能把焊死(rps 0 = 全量排队)或自相矛盾的阈值写进 ConfigMap"
        }

        $script:PeakShavingLaunchTier = & $newTier
        $settings = Get-GatewayRateLimitSettings
        Assert-Equal -Expected 'enabled,zone-default-rps,zone-default-burst,ip-rps,ip-burst,account-cooldown-ms' -Actual (@($settings.Keys) -join ',') -Because '合法常量表不 throw,按固定顺序返回 6 个 Spring 属性名'
        $script:GatewayRateLimitOn = $false
        Assert-True -Condition ($null -eq (Get-GatewayRateLimitSettings)) -Because '限流关时返回 $null(什么都不写,落回 jar 内的 enabled: false)'

        # 排队容量是常量表的另一项(Go 侧 uint32),由 New-LoginQueueYaml 守。它还要读 go/login/etc/login.yaml 的 Queue.* 镜像键:
        # Get-AuthoritativeScalar 按 $RepoRoot 定位仓库(在 k8s_deploy.ps1 里是脚本级变量,这里按动态作用域给)。
        $RepoRoot = Get-RepoRoot
        $script:LoginQueueOn = $true
        foreach ($badCapacity in @(0, 4294967296)) {
            $tier = & $newTier
            $tier['LoginQueueZoneCapacity'] = $badCapacity
            $script:PeakShavingLaunchTier = $tier
            $message = ''
            try { New-LoginQueueYaml -CurrentZoneId 101 | Out-Null } catch { $message = $_.Exception.Message }
            Assert-Match -Text $message -Pattern ([regex]::Escape("LoginQueueZoneCapacity=$badCapacity 不在 1..4294967295")) -Because "排队容量 $badCapacity 越界必须 throw 并点名常量与值(0 = 本 zone 永远放不出人;超过 uint32 在 Go 侧解析失败)"
        }
        $script:PeakShavingLaunchTier = & $newTier
        $queueYaml = New-LoginQueueYaml -CurrentZoneId 101
        Assert-Match -Text $queueYaml -Pattern '(?m)^    "101": 5000\s*$' -Because '合法常量表不 throw,ZoneCapacityOverride 只写本 zone 的开服档容量'
        $script:LoginQueueOn = $false
        Assert-Equal -Expected '' -Actual (New-LoginQueueYaml -CurrentZoneId 101) -Because '排队关时返回空串(不写 Queue 段,落回 login.yaml 的 Enabled: false)'
    }
    finally {
        Remove-Variable -Name GatewayRateLimitOn, LoginQueueOn, PeakShavingLaunchTier -Scope Script -ErrorAction SilentlyContinue
    }

    # 「在任何写操作之前」:写路径入口的 Show-PeakShavingPlan(先于 Initialize-InjectedSecrets 与任何 kubectl 写操作)在本次部署
    # gateway 且限流开时先求一次 Get-GatewayRateLimitSettings,部署 login 且排队开时先求一次 New-LoginQueueYaml(容量越界、login.yaml
    # 缺 Queue 键都在这里 throw)。否则自检要等到生成 gateway / login ConfigMap 才跑,那时同一 zone 的 node ConfigMap / gate / scene
    # 已经 apply 过了,留下半截部署(spec_G4b §5 表头「部署期,都在任何 kubectl 写操作之前」)。
    $planFn = $deadlineDeployAst.Find({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -eq 'Show-PeakShavingPlan' }, $true)
    Assert-True -Condition ($null -ne $planFn) -Because 'k8s_deploy.ps1 应当有 Show-PeakShavingPlan(写路径入口的削峰计划)'
    $selfCheckCall = $planFn.Body.Find({ param($a) $a -is [System.Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'Get-GatewayRateLimitSettings' }, $true)
    Assert-True -Condition ($null -ne $selfCheckCall) -Because 'Show-PeakShavingPlan 必须调用 Get-GatewayRateLimitSettings,把常量表自检提前到写路径入口'
    $queueSelfCheckCall = $planFn.Body.Find({ param($a) $a -is [System.Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'New-LoginQueueYaml' }, $true)
    Assert-True -Condition ($null -ne $queueSelfCheckCall) -Because 'Show-PeakShavingPlan 必须调用 New-LoginQueueYaml,把排队容量越界与 login.yaml 缺 Queue 键的检查提前到写路径入口'
    $planAt = $prodRun.Output.IndexOf('PeakShaving: ')
    $firstKubectlAt = $prodRun.Output.IndexOf('[dry-run] kubectl')
    Assert-True -Condition ($planAt -ge 0 -and ($firstKubectlAt -lt 0 -or $planAt -lt $firstKubectlAt)) -Because '削峰计划(含常量表自检)必须先于任何 kubectl 写操作打出'
}

Test-Case 'gateway 削峰指纹随 Redis 口令变化(轮换口令自动滚动 gateway)' {
    # Lettuce 只在启动时读 spring.data.redis.password:轮换 MMORPG_REDIS_PASSWORD 而 gateway 不滚动,旧口令被拒后限流一律 fail-open。
    # 所以口令是 gateway 削峰指纹的输入之一;管理面口令指纹只由管理面口令决定,不该跟着变。
    Assert-Equal -Expected 0 -Actual $peakGwRedisRotRun.ExitCode -Because "换 Redis 口令的 DryRun 应当成功。输出: $($peakGwRedisRotRun.Output)"
    $rotated = Get-PeakShavingAnnotation -Output $peakGwRedisRotRun.Output -Workload 'gateway' -Label '换 Redis 口令'
    $baseline = Get-PeakShavingAnnotation -Output $prodRun.Output -Workload 'gateway' -Label 'prod 基线'
    Assert-True -Condition ($rotated.Peak -ne $baseline.Peak) -Because "Redis 口令变了,gateway 削峰指纹必须变(两者都是 $($baseline.Peak))"
    Assert-Equal -Expected $baseline.AdminKey -Actual $rotated.AdminKey -Because '管理面口令没变,口令指纹必须不变(两条注解各管各的输入)'
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $peakGwRedisRotRun.Output -Name 'java-svc-gateway-config')
    Assert-True -Condition ((Get-FlatValue -Flat $flat -KeyPath 'data.application.yaml.spring.data.redis.password') -ceq $PeakRotatedRedisPassword) -Because 'ConfigMap 里的 Redis 口令必须是换过的新值(与指纹同一份输入)'
}

Test-Case 'gateway 削峰指纹随 rate-limit 段文本变化(只改 -GatewayTrustedProxies,密钥不变,也要自动滚动 gateway)' {
    # 指纹输入 = 整段 rate-limit 文本 + Redis 口令。上面几例只验证了「随口令变」「同输入同指纹」「关时消失」:指纹输入若退化成只含口令,
    # 它们照样全绿,而改阈值、改可信代理网段都不会再滚动 gateway(ConfigMap 只在启动时读)。这里密钥与 $prodRun 完全相同,只多一段代理网段。
    Assert-Equal -Expected 0 -Actual $peakGwProxyRun.ExitCode -Because "只加 -GatewayTrustedProxies 的 prod DryRun 应当成功。输出: $($peakGwProxyRun.Output)"
    $gatewayCm = Select-ManifestByName -Output $peakGwProxyRun.Output -Name 'java-svc-gateway-config'
    Assert-Match -Text $gatewayCm -Pattern ('(?m)^[ ]+trusted-proxies:[ ]*\r?\n[ ]+- "' + [regex]::Escape($PeakTrustedProxies) + '"') -Because '夹具前提:rate-limit 段里要真的多出这段可信代理网段,否则下面「指纹变了」无从谈起'
    $withProxies = Get-PeakShavingAnnotation -Output $peakGwProxyRun.Output -Workload 'gateway' -Label '加可信代理'
    $baseline = Get-PeakShavingAnnotation -Output $prodRun.Output -Workload 'gateway' -Label 'prod 基线'
    Assert-True -Condition ($withProxies.Peak -ne $baseline.Peak) -Because "rate-limit 段文本变了(多了 trusted-proxies),gateway 削峰指纹必须变(两者都是 $($baseline.Peak))"
    Assert-Equal -Expected $baseline.AdminKey -Actual $withProxies.AdminKey -Because '管理面口令没变,口令指纹必须不变(两条注解各管各的输入)'
}

# 登录排队合入门槛(用例 19–22,2026-10-15;docs/handoff/launch-readiness-20261013/spec_G4b.md §7「合入门槛」)。
# G4b-2 的 login 段已先于门槛入库,k8s_deploy.ps1 用常量 $script:LoginQueueMergeGateCleared 挡住打开排队;未清时 Resolve-LoginQueueSwitch
# 把 auto 一律解析为关、把 on 拒在任何写操作之前。测试接缝 MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED=1 只在 -DryRun 下视为门槛已清。
# 「不带 -DryRun + 接缝变量 → 拒绝」只在用例 21 里对纯函数断言:不带 -DryRun 起部署子进程,万一没拒绝就会对本机当前 kubectl context
# 真执行,所以本文件不起这种子进程。用例 19 / 20 断的是门槛未清时的产物,负责人把常量改为 $true 的同一提交里要随之改写这两例
# (届时 staging auto = 开、-LoginQueue on 放行);刻意不写「常量必须为 $false」的断言,那会把放开门槛本身挡在测试上。
# 用例 21 按 AST 抽出下面两个函数直接调用(与上文 deadline 守卫、常量表自检同一做法)。
$loginQueueGateFunctionNames = @('Resolve-PeakShavingSwitch', 'Resolve-LoginQueueSwitch')
foreach ($loginQueueGateFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($loginQueueGateFunctionNames -contains $loginQueueGateFnAst.Name) {
        Set-Item -Path "Function:script:$($loginQueueGateFnAst.Name)" -Value $loginQueueGateFnAst.Body.GetScriptBlock()
    }
}
foreach ($loginQueueGateFnName in $loginQueueGateFunctionNames) {
    if (-not (Test-Path "Function:$loginQueueGateFnName")) { throw "k8s_deploy.ps1 缺少函数 $loginQueueGateFnName(登录排队的合入门槛被删了?后续断言无意义)" }
}

Test-Case '合入门槛未清(不带测试接缝):staging zone-up 的 -LoginQueue auto 解析为关,打 LoginQueueGate=closed,login 不写 Queue 段、不要求排队密钥,限流照开' {
    # 用例 19。门槛清掉后本例要随常量一起改(见上方说明)。
    # 显式置空接缝变量:不受跑测机器上残留的同名环境变量影响。排队密钥也置空:门槛未清时排队关,不该要求它。
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED'] = ''
    $env2['MMORPG_LOGIN_QUEUE_TOKEN_SECRET'] = ''
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-ReleaseProfile', 'staging', '-SkipPreflight', '-NodeImage', $PeakNodeImage)) -Env $env2
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "门槛未清时 staging 排队关,缺 MMORPG_LOGIN_QUEUE_TOKEN_SECRET 也必须能部署。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern 'PeakShaving: gateway_rate_limit=on\(-GatewayRateLimit auto\) login_queue=off\(-LoginQueue auto\) profile=staging ' -Because '计划行:限流仍按档位开,排队因门槛未清解析为关'
    Assert-Match -Text $run.Output -Pattern '\[PeakShaving\]LoginQueueGate=closed' -Because '门槛把本该开的排队关掉时,要用专门的警告点明原因与放开方法'
    Assert-NotMatch -Text $run.Output -Pattern '\[PeakShaving\]LoginQueue=off' -Because '门槛未清时不打 LoginQueue=off:它说的「不粘滞、下次部署按档位恢复」在门槛未清时不成立'
    $loginFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $run.Output -Name 'go-svc-login-config')
    $queueKeys = @(@(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Queue') + @(Get-FlatSubtreeKeys -Flat $loginFlat -Path 'data.login.yaml.Secrets.QueueToken'))
    Assert-True -Condition ($queueKeys.Count -eq 0) -Because "门槛未清时 login ConfigMap 不得写 Queue / Secrets.QueueToken(实际出现:$($queueKeys -join ', '))"
    # 按「kind: Deployment + metadata 里的名字」挑 login 的 Deployment(同上文命令 topic 用例的挑法)。
    $loginDeployment = @(Get-ManifestBlocks -Output $run.Output | Where-Object {
        $_ -cmatch '(?m)^kind: Deployment\s*$' -and $_ -cmatch '(?m)^  name: login\s*$'
    }) | Select-Object -First 1
    Assert-True -Condition ($null -ne $loginDeployment) -Because 'DryRun 输出里应当有 login 的 Deployment(本例断它不带削峰指纹的前提)'
    Assert-NotMatch -Text $loginDeployment -Pattern 'peak-shaving-hash' -Because '排队关 = login 的 Pod 模板不带削峰指纹'
    $gatewayFlat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $run.Output -Name 'java-svc-gateway-config')
    Assert-Equal -Expected 'true' -Actual (Get-FlatValue -Flat $gatewayFlat -KeyPath 'data.application.yaml.gate.rate-limit.enabled') -Because '合入门槛只管登录排队,gateway 限流仍按档位开'
    $null = Get-PeakShavingAnnotation -Output $run.Output -Workload 'gateway' -Label '门槛未清 staging'
}

Test-Case '合入门槛未清(不带测试接缝):prod -LoginQueue on 在参数解析后、任何 kubectl 之前拒绝并点名门槛' {
    # 用例 20。门槛清掉后本例要随常量一起改(见上方说明)。其余生产密钥都给齐:门槛已清时这次 DryRun 会成功,拒绝只能来自门槛。
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED'] = ''
    $run = Invoke-DeployDryRun -Arguments ($ProdArgs + @('-LoginQueue', 'on')) -Env $env2
    Assert-True -Condition ($run.ExitCode -ne 0) -Because "门槛未清时 -LoginQueue on 必须拒绝。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern '-LoginQueue on 被拒绝' -Because '错误要说清是哪个参数值被拒'
    Assert-Match -Text $run.Output -Pattern '合入门槛未清' -Because '错误要说清拒绝原因是合入门槛'
    Assert-Match -Text $run.Output -Pattern 'LoginQueueMergeGateCleared' -Because '错误要点名放开门槛要改的常量'
    Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl' -Because '门槛在参数解析后就拒绝,早于任何 kubectl(含只读)'
    # Release: 行与削峰计划行都在写路径入口才打,拒绝发生在它们之前 = 拒绝点在参数解析后(Resolve-LoginQueueSwitch 的调用处)。
    Assert-NotMatch -Text $run.Output -Pattern '(?m)^Release: profile=' -Because '拒绝必须早于 Release: 行(参数解析后、加载库之前)'
    Assert-NotMatch -Text $run.Output -Pattern '(?m)^PeakShaving: ' -Because '拒绝必须早于削峰计划行'
}

Test-Case '合入门槛纯函数:Resolve-LoginQueueSwitch 全组合(未清时 auto / off 恒关、on 拒绝;已清时与 Resolve-PeakShavingSwitch 逐格一致;测试接缝只在 DryRun 生效)' {
    # 用例 21。不依赖常量当前的值:常量以 -MergeGateCleared 参数传入。
    $profiles = @('dev', 'staging', 'prod')
    # 对照表本身先钉住:Resolve-PeakShavingSwitch 被改坏时,下面「与它一致」就失去意义。
    foreach ($profileName in $profiles) {
        Assert-Equal -Expected ($profileName -ne 'dev') -Actual (Resolve-PeakShavingSwitch -Value 'auto' -ReleaseProfileName $profileName) -Because "Resolve-PeakShavingSwitch:auto 在 $profileName 档应为 $($profileName -ne 'dev')(dev 关,其余开)"
        Assert-Equal -Expected $true -Actual (Resolve-PeakShavingSwitch -Value 'on' -ReleaseProfileName $profileName) -Because "Resolve-PeakShavingSwitch:on 在 $profileName 档应为开"
        Assert-Equal -Expected $false -Actual (Resolve-PeakShavingSwitch -Value 'off' -ReleaseProfileName $profileName) -Because "Resolve-PeakShavingSwitch:off 在 $profileName 档应为关"
    }

    # 门槛未清(常量 $false、不带接缝):DryRun 与否结论相同。'ON':ValidateSet 不分大小写,大写的 on 也必须被拒。
    foreach ($dryRunMode in @($false, $true)) {
        foreach ($profileName in $profiles) {
            foreach ($value in @('auto', 'off')) {
                $actual = Resolve-LoginQueueSwitch -Value $value -ReleaseProfileName $profileName -MergeGateCleared $false -DryRunMode $dryRunMode -TestOverride ''
                Assert-Equal -Expected $false -Actual $actual -Because "门槛未清时 -LoginQueue $value 在 $profileName 档必须解析为关(DryRun=$dryRunMode)"
            }
            foreach ($value in @('on', 'ON')) {
                $message = ''
                try { $null = Resolve-LoginQueueSwitch -Value $value -ReleaseProfileName $profileName -MergeGateCleared $false -DryRunMode $dryRunMode -TestOverride '' }
                catch { $message = $_.Exception.Message }
                Assert-Match -Text $message -Pattern '合入门槛未清' -Because "门槛未清时 -LoginQueue $value 在 $profileName 档必须 throw(DryRun=$dryRunMode)"
                Assert-Match -Text $message -Pattern 'LoginQueueMergeGateCleared' -Because '错误要点名放开门槛要改的常量'
                Assert-Match -Text $message -Pattern 'spec_G4b\.md §7' -Because '错误要给出三项门槛的出处'
                Assert-Match -Text $message -Pattern 'spec_G4b-G\.md §8\.4' -Because '错误要点名真实 Redis 冒烟那一项的出处'
            }
        }
    }

    # 门槛已清的三种来源,结论都与 Resolve-PeakShavingSwitch 逐格一致。
    foreach ($cleared in @(
        @{ Label = '常量 $true、非 DryRun'; MergeGateCleared = $true; DryRunMode = $false; TestOverride = '' }
        @{ Label = '常量 $true、DryRun'; MergeGateCleared = $true; DryRunMode = $true; TestOverride = '' }
        @{ Label = '常量 $false、DryRun + 接缝 1'; MergeGateCleared = $false; DryRunMode = $true; TestOverride = '1' }
    )) {
        foreach ($profileName in $profiles) {
            foreach ($value in @('auto', 'on', 'off', 'ON')) {
                $expected = Resolve-PeakShavingSwitch -Value $value -ReleaseProfileName $profileName
                $actual = Resolve-LoginQueueSwitch -Value $value -ReleaseProfileName $profileName -MergeGateCleared $cleared.MergeGateCleared -DryRunMode $cleared.DryRunMode -TestOverride $cleared.TestOverride
                Assert-Equal -Expected $expected -Actual $actual -Because "$($cleared.Label):-LoginQueue $value 在 $profileName 档必须与 Resolve-PeakShavingSwitch 一致"
            }
        }
    }

    # 接缝不带 -DryRun:只要设置了就 throw,常量为 $true 时也一样(fail-closed,它放不开真实部署)。
    foreach ($mergeGateCleared in @($false, $true)) {
        foreach ($override in @('1', '0', 'true')) {
            $message = ''
            try { $null = Resolve-LoginQueueSwitch -Value 'auto' -ReleaseProfileName 'prod' -MergeGateCleared $mergeGateCleared -DryRunMode $false -TestOverride $override }
            catch { $message = $_.Exception.Message }
            Assert-Match -Text $message -Pattern 'MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED' -Because "不带 -DryRun 时设置了接缝变量(值 '$override',常量 $mergeGateCleared)必须 throw 并点名变量"
            Assert-Match -Text $message -Pattern '只在 -DryRun 下生效' -Because '错误要说清接缝只在 -DryRun 下生效'
        }
    }
    # 接缝在 DryRun 下只接受 1,其它值不猜含义。
    foreach ($override in @('0', 'true', ' 1')) {
        $message = ''
        try { $null = Resolve-LoginQueueSwitch -Value 'auto' -ReleaseProfileName 'prod' -MergeGateCleared $false -DryRunMode $true -TestOverride $override }
        catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern '只接受 1' -Because "DryRun 下接缝变量为 '$override' 必须 throw(只接受 1)"
    }
    # 未设置:子进程里没设的环境变量读出来是 $null,与空串同样视为未设置。
    Assert-Equal -Expected $false -Actual (Resolve-LoginQueueSwitch -Value 'auto' -ReleaseProfileName 'prod' -MergeGateCleared $false -DryRunMode $false -TestOverride $null) -Because '接缝变量未设置($null)且门槛未清时,prod auto 解析为关、不 throw'
}

Test-Case '合入门槛源码形状:$script:LoginQueueOn 只由 Resolve-LoginQueueSwitch 赋值,门槛常量只能是字面量,测试接缝只读一处且绑本脚本的 -DryRun' {
    # 用例 22。用例 21 证明了纯函数本身;这里证明脚本真的经它取值、门槛参数绑的是常量与 -DryRun,不留绕过的路。
    $assignmentsTo = {
        param([string]$Target)
        @($deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.AssignmentStatementAst] -and $a.Left.Extent.Text -ieq $Target }, $true))
    }
    $topLevel = @($deadlineDeployAst.EndBlock.Statements)

    $queueAssigns = & $assignmentsTo '$script:LoginQueueOn'
    Assert-Equal -Expected 1 -Actual $queueAssigns.Count -Because 'k8s_deploy.ps1 里 $script:LoginQueueOn 只能有一处赋值(多一处就可能绕过合入门槛)'
    $queueAssign = $queueAssigns[0]
    Assert-True -Condition ($topLevel -contains $queueAssign) -Because '$script:LoginQueueOn 必须在脚本顶层赋值(参数解析后算一次)'
    $calls = @($queueAssign.Right.FindAll({ param($a) $a -is [System.Management.Automation.Language.CommandAst] }, $true))
    Assert-True -Condition ($calls.Count -eq 1 -and $calls[0].GetCommandName() -eq 'Resolve-LoginQueueSwitch') -Because "`$script:LoginQueueOn 必须只由 Resolve-LoginQueueSwitch 算出(实际:$($queueAssign.Right.Extent.Text))"
    $callText = $calls[0].Extent.Text
    Assert-Match -Text $callText -Pattern '-MergeGateCleared[:\s]\s*\$script:LoginQueueMergeGateCleared(\s|$)' -Because '-MergeGateCleared 必须绑门槛常量 $script:LoginQueueMergeGateCleared'
    Assert-Match -Text $callText -Pattern '-DryRunMode[:\s]\s*\$DryRun\.IsPresent(\s|$)' -Because '-DryRunMode 必须绑本脚本的 -DryRun(测试接缝只在 DryRun 下生效的前提)'
    Assert-Match -Text $callText -Pattern '-TestOverride[:\s]\s*\$env:MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED(\s|$)' -Because '-TestOverride 必须读测试接缝变量本身'
    Assert-Match -Text $callText -Pattern '-Value[:\s]\s*\$LoginQueue(\s|$)' -Because '-Value 必须是 -LoginQueue 参数'

    # $DryRun 必须就是参数块里的 [switch]$DryRun,脚本里没有别处给它赋值。
    $dryRunParams = @($deadlineDeployAst.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'DryRun' })
    Assert-True -Condition ($dryRunParams.Count -eq 1 -and $dryRunParams[0].StaticType -eq [System.Management.Automation.SwitchParameter]) -Because 'k8s_deploy.ps1 的参数块里应当有 [switch]$DryRun'
    $dryRunAssigns = @($deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.AssignmentStatementAst] -and $a.Left.Extent.Text -match '(?i)\$(script:)?DryRun$' }, $true))
    Assert-Equal -Expected 0 -Actual $dryRunAssigns.Count -Because '脚本里不得给 $DryRun 赋值(否则接缝的生效条件就不再是调用方给的 -DryRun)'

    # 测试接缝变量只在上面那一处调用里读,也没有经 GetEnvironmentVariable / Env: 路径之类的字面量另读一次。
    $envReads = @($deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.VariableExpressionAst] -and $a.VariablePath.UserPath -ieq 'env:MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED' }, $true))
    Assert-Equal -Expected 1 -Actual $envReads.Count -Because '$env:MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED 只能读一处'
    Assert-True -Condition ($envReads[0].Extent.StartOffset -ge $calls[0].Extent.StartOffset -and $envReads[0].Extent.EndOffset -le $calls[0].Extent.EndOffset) -Because '接缝变量唯一的读取点必须在 Resolve-LoginQueueSwitch 的调用里'
    $nameLiterals = @($deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.StringConstantExpressionAst] -and $a.Value -imatch '^(env:)?MMORPG_TEST_LOGIN_QUEUE_GATE_CLEARED$' }, $true))
    Assert-Equal -Expected 0 -Actual $nameLiterals.Count -Because '不得以字符串字面量(GetEnvironmentVariable / Env: 路径)另读接缝变量'

    # 门槛常量:恰好一处、顶层、字面量 $true / $false,先于 $script:LoginQueueOn 的赋值。不断言它当前的值(放开门槛只改这一处)。
    $gateAssigns = & $assignmentsTo '$script:LoginQueueMergeGateCleared'
    Assert-Equal -Expected 1 -Actual $gateAssigns.Count -Because '$script:LoginQueueMergeGateCleared 只能有一处赋值'
    Assert-True -Condition ($topLevel -contains $gateAssigns[0]) -Because '门槛常量必须在脚本顶层赋值'
    Assert-Match -Text $gateAssigns[0].Right.Extent.Text -Pattern '^\$(true|false)$' -Because "门槛常量只能是字面量 `$true / `$false,不能改成读参数或环境变量(实际:$($gateAssigns[0].Right.Extent.Text))"
    Assert-True -Condition ($gateAssigns[0].Extent.StartOffset -lt $queueAssign.Extent.StartOffset) -Because '门槛常量必须先于 $script:LoginQueueOn 的赋值'

    # 早于任何写操作:排在顶层写路径门禁块 if ($Command -in @("zone-up", "all-up", "infra-up")) 之前(Show-PeakShavingPlan、密钥解析与 kubectl 都在它之后)。
    $writeGates = @($topLevel | Where-Object {
        $_ -is [System.Management.Automation.Language.IfStatementAst] -and
        $_.Clauses[0].Item1.Extent.Text -match '^\$Command\s+-in\s+@\(\s*"zone-up"\s*,\s*"all-up"\s*,\s*"infra-up"\s*\)$'
    })
    Assert-Equal -Expected 1 -Actual $writeGates.Count -Because 'k8s_deploy.ps1 顶层应当恰好有一个写路径门禁块'
    Assert-True -Condition ($queueAssign.Extent.StartOffset -lt $writeGates[0].Extent.StartOffset) -Because '$script:LoginQueueOn 的赋值(-LoginQueue on 的拒绝点)必须先于写路径门禁块'

    # 限流不受合入门槛影响:仍由 Resolve-PeakShavingSwitch 按档位算。
    $gatewayAssigns = & $assignmentsTo '$script:GatewayRateLimitOn'
    Assert-Equal -Expected 1 -Actual $gatewayAssigns.Count -Because '$script:GatewayRateLimitOn 只能有一处赋值'
    $gatewayCalls = @($gatewayAssigns[0].Right.FindAll({ param($a) $a -is [System.Management.Automation.Language.CommandAst] }, $true))
    Assert-True -Condition ($gatewayCalls.Count -eq 1 -and $gatewayCalls[0].GetCommandName() -eq 'Resolve-PeakShavingSwitch') -Because '$script:GatewayRateLimitOn 仍只由 Resolve-PeakShavingSwitch 算出(合入门槛只管登录排队)'
}

# ─────────────────────────────────────────────────────────────────
# 4. 负向用例 —— 断错误文本,不只断退出码
# ─────────────────────────────────────────────────────────────────

Test-Case "负向:prod 档位缺 MMORPG_GATE_TOKEN_SECRET 必须 fail-closed 并点名该变量" {
    $env2 = @{}
    foreach ($k in $ProdEnv.Keys) { $env2[$k] = $ProdEnv[$k] }
    $env2['MMORPG_GATE_TOKEN_SECRET'] = ''
    $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "缺密钥必须阻断"
    Assert-Match -Text $run.Output -Pattern 'MMORPG_GATE_TOKEN_SECRET' -Because "错误必须点名缺哪个环境变量,否则运维只能猜"
    Assert-Match -Text $run.Output -Pattern '要求从环境变量' -Because "错误文本要说清是注入缺失,不是别的失败"
}

Test-Case "负向:prod 档位环境变量仍是占位串时必须拒绝" {
    $env2 = @{}
    foreach ($k in $ProdEnv.Keys) { $env2[$k] = $ProdEnv[$k] }
    $env2['MMORPG_MYSQL_PASSWORD'] = 'change-me-in-production'
    $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "占位串必须阻断"
    Assert-Match -Text $run.Output -Pattern '仍是占位串' -Because "错误文本要指出是占位串问题"
    Assert-Match -Text $run.Output -Pattern 'MMORPG_MYSQL_PASSWORD' -Because "要点名是哪个变量"
}

Test-Case "负向:prod 档位密钥长度不达标时必须拒绝" {
    $env2 = @{}
    foreach ($k in $ProdEnv.Keys) { $env2[$k] = $ProdEnv[$k] }
    $env2['MMORPG_GATE_TOKEN_SECRET'] = 'short'
    $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "短密钥必须阻断"
    Assert-Match -Text $run.Output -Pattern '长度 5 <' -Because "错误文本要给出实际长度和门槛"
}

Test-Case "负向:prod 档位传 latest 镜像必须被拒并说明回滚失效的理由" {
    $args2 = $BaseArgs + @("-ReleaseProfile", "prod", "-SkipPreflight", "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:latest")
    $run = Invoke-DeployDryRun -Arguments $args2 -Env $ProdEnv

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "prod 不许可变 tag"
    Assert-Match -Text $run.Output -Pattern '可变 tag' -Because "错误文本要点出可变 tag"
    Assert-Match -Text $run.Output -Pattern 'rollout undo' -Because "错误文本要说清后果是回滚失效"
}

Test-Case "负向:prod 档位不加 -SkipPreflight 时预检必须真的被调用并阻断" {
    # 这条测的是"检查器有没有调用方"。仓库当前配置里 GateTokenSecret 等仍是占位串,
    # 所以预检必然 FAIL —— 如果这里居然过了,说明门禁根本没挂上。
    $args2 = $BaseArgs + @("-ReleaseProfile", "prod", "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:0123456789ab")
    $run = Invoke-DeployDryRun -Arguments $args2 -Env $ProdEnv

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "预检未过必须阻断部署"
    Assert-Match -Text $run.Output -Pattern 'release preflight' -Because "错误必须来自发布预检,而不是别的失败路径"
    Assert-Match -Text $run.Output -Pattern '部署被阻断' -Because "错误文本要说清部署被拦住了"
}

Test-Case "负向:zone-down / zone-status 不受发布门禁影响(止血路径不能被挡)" {
    $run = Invoke-DeployDryRun -Arguments @("-Command", "zone-status", "-ZoneName", "contract-test", "-DryRun", "-ReleaseProfile", "prod", "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:latest")
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "出事时看状态/关 zone 绝不能被发布预检挡住。输出: $($run.Output)"
}

# ─────────────────────────────────────────────────────────────────
# 5. 灾难回滚脚本
# ─────────────────────────────────────────────────────────────────

Test-Case "负向:k8s_zone_rollback 拒绝可变 tag 作为回滚目标" {
    $run = Invoke-ToolScript -ScriptName "k8s_zone_rollback.ps1" -Arguments @(
        "-ZoneName", "contract-test", "-ZoneId", "101",
        "-TargetTime", "2026-05-15T14:23:00Z",
        "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:latest"
    )
    Assert-True -Condition ($run.ExitCode -ne 0) -Because "灾难回滚指向可变 tag = 数据回档了、代码没回"
    Assert-Match -Text $run.Output -Pattern '灾难回滚拒绝' -Because "错误文本要点明是回滚脚本自己拒的"
}

Test-Case "k8s_zone_rollback 接受不可变 tag(dry-run 走完全流程)" {
    $run = Invoke-ToolScript -ScriptName "k8s_zone_rollback.ps1" -Arguments @(
        "-ZoneName", "contract-test", "-ZoneId", "101",
        "-TargetTime", "2026-05-15T14:23:00Z",
        "-NodeImage", "ghcr.io/luyuancpp/mmorpg-node:0123456789ab"
    )
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dry-run 不该失败。输出: $($run.Output)"
    Assert-Match -Text $run.Output -Pattern 'DRY-RUN' -Because "默认必须是 dry-run"
}

# battle 全局池与命令 topic 的跨文件部署契约。
Test-Case 'node ConfigMap 的 Kafka 命令 topic 世代和分区数必须与预建 Job 同源' {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $devOut -Name 'node-config')
    foreach ($key in @('CommandTopicPartitions', 'CommandTopicGeneration')) {
        Assert-Equal -Expected (Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath "Kafka.$key") -Actual (Get-FlatValue -Flat $flat -KeyPath "data.base_deploy_config.yaml.Kafka.$key") -Because '挂载 CM 后的消费者必须使用预建 topic 的同一寻址契约'
    }
}
$BattleInfraArgs = @('-Command', 'infra-up', '-DryRun', '-SkipPreflight', '-SkipGoSvc', '-SkipJavaSvc', '-WaitReady', '-InfraNamespace', 'contract-battle-infra', '-NodeImage', 'registry.invalid/test/mmorpg-node:0123456789ab')
$BattleTestEnv = @{ MMORPG_BATTLE_TOKEN_SECRET = 'contract-battle-ticket-0123456789abcdef012345'; MMORPG_GATE_TOKEN_SECRET = 'contract-gate-ticket-0123456789abcdef012345' }
$battleInfraRun = Invoke-DeployDryRun -Arguments $BattleInfraArgs -Env $BattleTestEnv
# P0-B 批 2(docs/handoff/global-singletons-p0-findings-20261014.md):node ConfigMap 的 Kafka.GroupID 从 bin/etc 镜像,不再由
# 生成器自带常数;K8s 上同批关掉旧的一节点一 topic 消费路径。紧跟上一条命令 topic 用例,只是要等 $battleInfraRun 就位,
# 好同时核 battle-node-config —— battle 只生产不消费,旧二进制在 GroupID 非空时拿不到生产端 broker,出事的正是它。
# DisableLegacyPerNodeTopic 是 K8s 模板常数(本机 bin/etc 那份仍是 false、由运维显式翻),所以断言字面值 true,不与 bin/etc 比。
Test-Case 'node ConfigMap 的 Kafka.GroupID 从 bin/etc 镜像、不再写死共享组名,K8s 上关闭旧的一节点一 topic 路径(node-config 与 battle-node-config)' {
    $expectedGroupId = Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'Kafka.GroupID'
    foreach ($target in @(
        @{ Name = 'node-config'; Output = $devOut },
        @{ Name = 'battle-node-config'; Output = $battleInfraRun.Output }
    )) {
        $block = Select-ManifestByName -Output $target.Output -Name $target.Name
        $flat = ConvertTo-FlatManifest -Block $block
        Assert-Equal -Expected $expectedGroupId -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.Kafka.GroupID') -Because "$($target.Name) 整目录遮蔽镜像里的 bin/etc,GroupID 必须等于 bin/etc 的权威值(允许空串)。以前写死共享组名:旧二进制的 battle 拿不到生产端 broker,gate / scene 的旧路径全服挤进同一个消费组"
        Assert-NotMatch -Text $block -Pattern 'game-consumer-group' -Because "$($target.Name) 不得再出现生成器自带的共享组名常数(2026-05-27 本机改空后模板一直没跟)"
        Assert-Equal -Expected 'true' -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.Kafka.DisableLegacyPerNodeTopic') -Because "$($target.Name) 必须关掉旧路径:与 GroupID 镜像同批,否则旧路径换到新消费组后会从 earliest 重放旧 topic"
    }
}
Test-Case 'infra-up 默认部署一个 battle 并等待 rollout；zone-up 不重复部署全局池' {
    Assert-Equal -Expected 0 -Actual $battleInfraRun.ExitCode -Because 'battle 全局装配 DryRun 必须成功'
    $battle = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $battleInfraRun.Output -Name 'battle')
    Assert-Equal -Expected 1 -Actual (Get-FlatValue -Flat $battle -KeyPath 'spec.replicas') -Because '与本地默认一个 battle 一致'
    Assert-Equal -Expected 'Deployment' -Actual (Get-FlatValue -Flat $battle -KeyPath 'kind') -Because 'battle 由 infra 管理一个全局 Deployment'
    Assert-Match -Text $battleInfraRun.Output -Pattern 'rollout status deployment/battle -n contract-battle-infra' -Because 'WaitReady 必须包含 battle'
    Assert-True -Condition ($null -eq (Select-ManifestByName -Output $devOut -Name 'battle')) -Because 'zone-up 不得每区复制全局 battle 池'
}
Test-Case 'battle 部署保留 POD_IP 和独立 TCP/gRPC 端口，exec 传递退出信号' {
    $block = Select-ManifestByName -Output $battleInfraRun.Output -Name 'battle'
    Assert-Match -Text $block -Pattern 'args: \["mkdir -p /app/bin/logs/cpp_nodes && .*&& exec \./battle"\]' -Because 'shell 必须 exec，退出信号直接到 battle 主进程'
    Assert-Match -Text $block -Pattern 'fieldPath: status.podIP' -Because '发现必须通告实际 Pod IP'
    Assert-Match -Text $block -Pattern 'name: RPC_PORT\s+value: "20000"' -Because 'TCP 端口必须在非 gate 合法区间'
    Assert-Match -Text $block -Pattern 'name: NODE_PORT\s+value: "20000"' -Because '两种端口环境别名必须一致'
    Assert-Match -Text $block -Pattern 'containerPort: 50000\s+name: grpc' -Because 'gRPC=TCP+30000'
    Assert-Match -Text $block -Pattern 'startupProbe:\s+tcpSocket:\s+port: 20000' -Because '先等待客户端直连面监听'
    Assert-Match -Text $block -Pattern 'startupProbe:\s+tcpSocket:\s+port: 20000\s+periodSeconds: 2\s+failureThreshold: 150' -Because 'startup 预算 300s 必须越过 180s etcd 租约,否则同 Pod 重启撞旧注册时会被多杀几轮'
    Assert-Match -Text $block -Pattern 'readinessProbe:\s+tcpSocket:\s+port: 50000' -Because 'C++ 尚无 grpc.health.v1，必须使用可实现的 TCP 探针'
    Assert-NotMatch -Text $block -Pattern 'GATE_CLIENT_RPC_ROUTER|BATTLE_RUN_MODE' -Because '不能误注 gate 模式或关闭 battle 默认 prod 启动门禁'
}
Test-Case 'gate 就绪探针只探唯一的监听口 18000,不探不存在的 gRPC 口,也不加会杀容器的探针' {
    $block = Select-ManifestByName -Output $devOut -Name 'gate'
    Assert-Match -Text $block -Pattern 'readinessProbe:\s+tcpSocket:\s+port: 18000\s+periodSeconds: 5\s+failureThreshold: 3' -Because '玩家连接与节点 RPC 共用 18000,它 listen 即已注册进 etcd'
    Assert-NotMatch -Text $block -Pattern 'port: 48000' -Because 'gate 不注册 gRPC 服务,RpcPort+30000 上没有监听,探它会恒失败'
    Assert-NotMatch -Text $block -Pattern 'livenessProbe|startupProbe' -Because 'tcpSocket 看不出 EventLoop 卡死;startup 预算给不准会在同 Pod 重启时多杀几轮'
}

# gate 客户端 RPC 路由模式(turn-based §22 D75):K8s 部署层默认 "1",C++ 进程默认值与单测不改(D-12)。
# 部署层默认值只允许存在于 k8s_deploy.ps1 一处;这里钉住「默认 "1"、只注入 gate、"0" 回退仍可生成、拼错在入口就拒、
# 包装入口留空不覆盖」。翻转之前这个开关零覆盖,默认值被悄悄改回去不会有任何报错。
Test-Case 'gate 默认以路由模式部署:GATE_CLIENT_RPC_ROUTER="1" 只注入 gate,scene(Deployment / Fleet)不带' {
    $gate = Select-ManifestByName -Output $devOut -Name 'gate'
    Assert-Match -Text $gate -Pattern 'name: GATE_CLIENT_RPC_ROUTER\s+value: "1"' -Because 'K8s 默认路由模式(D75);省略该变量 gate 按 C++ 默认落回直连,chat / friend / trade 全部不可达'
    Assert-Equal -Expected 1 -Actual ([regex]::Matches($gate, 'name: GATE_CLIENT_RPC_ROUTER').Count) -Because 'gate 只该注入一次,重复的 env 键以哪条为准不该交给 kubelet 决定'
    Assert-Match -Text (Select-ManifestByName -Output $agonesOut -Name 'gate') -Pattern 'name: GATE_CLIENT_RPC_ROUTER\s+value: "1"' -Because 'scene 编排方式不影响 gate 的默认模式'
    Assert-NotMatch -Text (Select-ManifestByName -Output $devOut -Name 'scene') -Pattern 'GATE_CLIENT_RPC_ROUTER' -Because 'scene 不读这个变量,注入只会让人误以为它也分模式'
    Assert-NotMatch -Text (Select-ManifestByName -Output $agonesOut -Name 'scene') -Pattern 'GATE_CLIENT_RPC_ROUTER' -Because 'Agones Fleet 模板同样不得注入'
}
Test-Case '-GateRouterMode 0 回退路径仍可生成,gate 显式写出 "0"' {
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-GateRouterMode', '0', '-SkipGoSvc', '-SkipJavaSvc'))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "回退到直连模式的部署路径必须照样能生成。输出: $($run.Output)"
    Assert-Match -Text (Select-ManifestByName -Output $run.Output -Name 'gate') -Pattern 'name: GATE_CLIENT_RPC_ROUTER\s+value: "0"' -Because '"0" 也显式写出:kubectl 里一眼看出 gate 跑在直连模式,不靠猜 C++ 默认值'
}
Test-Case '负向:-GateRouterMode 只收 "0" / "1",拼错必须在入口被拒' {
    foreach ($bad in @('2', 'true')) {
        $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-GateRouterMode', $bad, '-SkipGoSvc', '-SkipJavaSvc'))
        Assert-True -Condition ($run.ExitCode -ne 0) -Because "gate 侧除 1/true/on 外一律当关;'$bad' 必须在脚本入口被拒,而不是原样写进 env 让两边字面值分家"
        Assert-Match -Text $run.Output -Pattern 'GateRouterMode' -Because '不能把其他执行错误误判为参数校验成功'
        Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl apply' -Because '参数校验必须先于任何资源变更'
    }
}
Test-Case '发现前缀:node-config 含 ClientRpcRouterNodeService.rpc;battle-node-config 保留 BattleNodeService.rpc' {
    Assert-Match -Text (Select-ManifestByName -Output $devOut -Name 'node-config') -Pattern '- "ClientRpcRouterNodeService\.rpc"' -Because '默认路由模式下 gate 的依赖门等 ClientRpcRouter,发现不到它 gate 永远过不了依赖门,登录 / 匹配全部 no_target'
    Assert-Match -Text (Select-ManifestByName -Output $battleInfraRun.Output -Name 'battle-node-config') -Pattern '- "BattleNodeService\.rpc"' -Because 'gate 已不连 battle(turn-based §22 D66),但 battle 自己按这些前缀 watch 自身节点键做劫持检测与注册自检,不能当死前缀删掉'
}

# 包装入口的透传:只从 AST 取出 dev_tools.ps1 / k8s_image.ps1 里的透传函数(照 dev_tools_merge_zone_contract.tests.ps1),
# 把 $ScriptDir 指到临时目录里的假下游脚本 —— 不起 kubectl、不 build 镜像,只看下游收没收到、收到什么。
function Get-ToolScriptFunctionText {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptName,
        [Parameter(Mandatory = $true)][string]$FunctionName
    )
    $path = Join-Path (Get-ToolsScriptsDir) $ScriptName
    $parseErrors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$null, [ref]$parseErrors)
    if ($parseErrors.Count) { throw "$ScriptName 解析失败: $($parseErrors | Out-String)" }
    $definition = $ast.Find({ param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $FunctionName }, $true)
    if ($null -eq $definition) { throw "$ScriptName 里找不到函数 $FunctionName(契约测试的前提被改掉了)" }
    return $definition.Extent.Text
}
function Invoke-GateRouterModePassthrough {
    param(
        [Parameter(Mandatory = $true)][string]$SourceScript,
        [Parameter(Mandatory = $true)][string]$WrapperFunction,
        [Parameter(Mandatory = $true)][hashtable]$WrapperArgs,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Mode
    )
    $fakeDir = Join-Path ([IO.Path]::GetTempPath()) ('mmorpg-gate-router-passthrough-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $fakeDir | Out-Null
    try {
        # 假下游刻意写成**简单脚本**(无 CmdletBinding / [Parameter]):包装函数 splat 过来的其余具名参数
        # 落进 $args,不会报"找不到参数";只把 GateRouterMode 的实收值打出来,没收到就是 <unset>。
        $fakeBody = 'param($GateRouterMode = "<unset>") "GateRouterMode=$GateRouterMode"'
        foreach ($downstream in @('k8s_deploy.ps1', 'k8s_image.ps1')) {
            Set-Content -LiteralPath (Join-Path $fakeDir $downstream) -Value $fakeBody -Encoding utf8
        }
        # 被测函数按动态作用域读调用方的 $ScriptDir / $GateRouterMode 等脚本级参数,这里放进本函数作用域;
        # 其余参数未定义即 $null,假下游不关心。k8s_image.ps1 的 Invoke-K8sDeploy 还要调 Get-ImageRef,给个替身。
        $ScriptDir = $fakeDir
        $GateRouterMode = $Mode
        function Get-ImageRef { return 'registry.invalid/test/mmorpg-node:0123456789ab' }
        . ([scriptblock]::Create((Get-ToolScriptFunctionText -ScriptName $SourceScript -FunctionName $WrapperFunction)))
        return (& $WrapperFunction @WrapperArgs | Out-String)
    }
    finally {
        Remove-Item -LiteralPath $fakeDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
Test-Case 'dev_tools / k8s_image 的 -GateRouterMode:留空不透传(由 k8s_deploy.ps1 默认值接管),显式给值才透传' {
    $wrappers = @(
        @{ Source = 'dev_tools.ps1'; Function = 'Invoke-K8sDeploy'; Args = @{ K8sCommand = 'zone-up' } },
        @{ Source = 'dev_tools.ps1'; Function = 'Invoke-K8sImage'; Args = @{ ImageCommand = 'release-zone' } },
        @{ Source = 'k8s_image.ps1'; Function = 'Invoke-K8sDeploy'; Args = @{ DeployCommand = 'zone-up' } }
    )
    foreach ($w in $wrappers) {
        $label = "$($w.Source) $($w.Function)"
        $unset = Invoke-GateRouterModePassthrough -SourceScript $w.Source -WrapperFunction $w.Function -WrapperArgs $w.Args -Mode ''
        Assert-Match -Text $unset -Pattern 'GateRouterMode=<unset>' -Because "$label 留空时不得透传:默认值只允许存在于 k8s_deploy.ps1 一处,透传空串还会被下游 ValidateSet 拒掉"
        $rollback = Invoke-GateRouterModePassthrough -SourceScript $w.Source -WrapperFunction $w.Function -WrapperArgs $w.Args -Mode '0'
        Assert-Match -Text $rollback -Pattern 'GateRouterMode=0' -Because "$label 必须能把回退值 0 透传下去,否则从包装入口无法回退到直连模式"
    }
    foreach ($target in @(@{ Name = 'dev_tools.ps1'; Command = 'help' }, @{ Name = 'k8s_image.ps1'; Command = 'list-refs' })) {
        $run = Invoke-ToolScript -ScriptName $target.Name -Arguments @('-Command', $target.Command, '-GateRouterMode', '2')
        Assert-True -Condition ($run.ExitCode -ne 0) -Because "$($target.Name) 的 -GateRouterMode 只收 空 / 0 / 1,拼错必须在入口被拒"
        Assert-Match -Text $run.Output -Pattern 'GateRouterMode' -Because '不能把其他执行错误误判为参数校验成功'
    }
}
Test-Case 'scene(Deployment)就绪探针探 gRPC 口 50000 并声明该端口;Agones Fleet 不加任何 K8s 探针' {
    $block = Select-ManifestByName -Output $devOut -Name 'scene'
    Assert-Match -Text $block -Pattern 'kind: Deployment' -Because '必须检查 Deployment 模式的 scene'
    Assert-Match -Text $block -Pattern 'containerPort: 50000\s+name: grpc' -Because 'gRPC=TCP+30000,探针端口必须有对应声明'
    Assert-Match -Text $block -Pattern 'readinessProbe:\s+tcpSocket:\s+port: 50000\s+periodSeconds: 5\s+failureThreshold: 3' -Because 'C++ 尚无 grpc.health.v1,用实际监听的 gRPC 口'
    Assert-NotMatch -Text $block -Pattern 'livenessProbe|startupProbe' -Because '会杀容器的探针在没有启动耗时实测前不加'
    $fleet = Select-ManifestByName -Output $agonesOut -Name 'scene'
    Assert-Match -Text $fleet -Pattern 'kind: Fleet' -Because '必须检查真正的 Fleet'
    Assert-NotMatch -Text $fleet -Pattern 'livenessProbe|startupProbe|readinessProbe' -Because 'GameServer Pod 是 restartPolicy: Never,杀容器 = 销毁 GameServer;就绪交给 Agones SDK'
}
Test-Case 'battle 配置必须满足独立票据密钥与权威连接上限' {
    $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $battleInfraRun.Output -Name 'battle-node-config')
    Assert-True -Condition ((Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.BattleTokenSecret') -ceq $BattleTestEnv.MMORPG_BATTLE_TOKEN_SECRET) -Because 'battle 密钥必须来自独立环境注入，测试不回显值'
    Assert-Equal -Expected (Get-EtcValue -RelativePath 'bin/etc/base_deploy_config.yaml' -KeyPath 'BattleMaxConnections') -Actual (Get-FlatValue -Flat $flat -KeyPath 'data.base_deploy_config.yaml.BattleMaxConnections') -Because '连接上限必须取权威配置'
    Assert-Match -Text (Select-ManifestByName -Output $battleInfraRun.Output -Name 'battle-node-config') -Pattern 'etcd.contract-battle-infra:2379' -Because 'battle 必须连接调用方指定的隔离 infra'
}
Test-Case 'battle-node-config 同样整块带 GrpcClient(battle 也是 C++ 节点,整目录挂载同样遮蔽镜像),且 infra-up 入口同样先过 deadline 预算门禁' {
    Assert-GrpcClientBlockMirrored -Output $battleInfraRun.Output -ConfigMapName 'battle-node-config'
    Assert-Match -Text $battleInfraRun.Output -Pattern 'GrpcClient deadline budget OK' -Because 'infra-up 与 zone-up 走同一个写路径门禁(Assert-GrpcClientDeadlineBudget)'
}
Test-Case 'BattleReplicas=0 不装配 battle，不删除现有池，也不要求 battle 密钥' {
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_BATTLE_TOKEN_SECRET'] = ''
    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-BattleReplicas','0','-ReleaseProfile','prod')) -Env $env2
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '显式不装配时不能要求无关密钥'
    Assert-True -Condition ($null -eq (Select-ManifestByName -Output $run.Output -Name 'battle')) -Because '0 不生成 Deployment'
    Assert-True -Condition ($null -eq (Select-ManifestByName -Output $run.Output -Name 'battle-node-config')) -Because '0 不生成 battle 配置'
    Assert-NotMatch -Text $run.Output -Pattern 'delete (deployment|deploy) battle' -Because '不装配不等于销毁已运行战斗'
}
Test-Case '负向：prod battle 缺密钥必须在任何 apply 之前失败' {
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_BATTLE_TOKEN_SECRET'] = ''
    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-ReleaseProfile','prod')) -Env $env2
    Assert-True -Condition ($run.ExitCode -ne 0) -Because '缺 battle 密钥必须拒绝部署'
    Assert-Match -Text $run.Output -Pattern 'MMORPG_BATTLE_TOKEN_SECRET' -Because '失败必须准确点名新密钥'
    Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl apply' -Because '密钥错误必须先于任何资源变更'
}
Test-Case '负向：battle 拒绝短密钥和与 gate 相同的密钥，即使发布档位是 dev' {
    foreach ($value in @('short', $BattleTestEnv.MMORPG_GATE_TOKEN_SECRET)) {
        $env2 = @{} + $BattleTestEnv
        $env2.MMORPG_BATTLE_TOKEN_SECRET = $value
        $run = Invoke-DeployDryRun -Arguments $BattleInfraArgs -Env $env2
        Assert-True -Condition ($run.ExitCode -ne 0) -Because 'K8s battle 运行时默认 prod，不能产出必然拒启的配置'
        Assert-Match -Text $run.Output -Pattern 'MMORPG_BATTLE_TOKEN_SECRET' -Because '错误必须点名配置键且不回显密钥值'
        Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl apply' -Because '失败必须发生于 apply 之前'
    }
}
Test-Case '负向：BattleReplicas 不能是负数' {
    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-BattleReplicas','-1')) -Env $BattleTestEnv
    Assert-True -Condition ($run.ExitCode -ne 0) -Because '负数必须由入口参数校验拒绝'
    Assert-Match -Text $run.Output -Pattern 'BattleReplicas' -Because '不能把其他执行错误误判为参数校验成功'
}
Test-Case 'all-up 多区只装配一次 battle 全局池，并尊重副本参数' {
    $run = Invoke-DeployDryRun -Arguments @('-Command','all-up','-DryRun','-SkipPreflight','-SkipGoSvc','-SkipJavaSvc','-ZonesConfigPath',(Join-Path (Get-RepoRoot) 'deploy/k8s/zones.sample.json'),'-InfraNamespace','contract-battle-infra','-BattleReplicas','2','-NodeImage','registry.invalid/test/mmorpg-node:0123456789ab') -Env $BattleTestEnv
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '完整多区纯生成必须通过'
    $blocks = @(Get-ManifestBlocks -Output $run.Output | Where-Object { $_ -match '(?m)^\s*name:\s*battle\s*$' })
    Assert-Equal -Expected 1 -Actual $blocks.Count -Because 'battle 不按 zone 复制'
    $flat = ConvertTo-FlatManifest -Block $blocks[0]
    Assert-Equal -Expected 2 -Actual (Get-FlatValue -Flat $flat -KeyPath 'spec.replicas') -Because '显式副本数必须生效'
}

Test-Case 'Kafka 控制器必须经发布未就绪地址的 headless 自举，避免 Service readiness 死锁' {
    Assert-Match -Text $battleInfraRun.Output -Pattern 'name: KAFKA_CONTROLLER_QUORUM_VOTERS\s+value: "1@kafka-0\.kafka-headless\.contract-battle-infra\.svc\.cluster\.local:9093"' -Because '控制器地址不能使用只包含 Ready endpoint 的普通 Service'
    Assert-Match -Text $battleInfraRun.Output -Pattern '(?s)name: kafka-headless.*?publishNotReadyAddresses: true' -Because 'headless 必须在 broker 未 Ready 时发布 Pod 地址'
    Assert-NotMatch -Text $battleInfraRun.Output -Pattern 'value: "1@kafka:9093"' -Because '不得保留导致启动死锁的旧自举地址'
}
# ─────────────────────────────────────────────────────────────────
# Kafka 拓扑(docs/design/no-single-node-horizontal-scaling-20261001.md §2):broker 数决定的派生值
# (选举组成员表 / 副本数 / min.insync.replicas / PDB / podManagementPolicy)只在 k8s_deploy.ps1 的
# Get-KafkaTopologyFor 里算一次。纯函数按 AST 抽出来直接喂值(与上面 deadline 守卫同一做法),渲染结果用 DryRun 核对。
$kafkaTopologyFunctionNames = @('Get-KafkaTopologyFor', 'Get-KafkaTopology', 'Get-KafkaTopologyChangeVerdict')
foreach ($kafkaFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($kafkaTopologyFunctionNames -contains $kafkaFnAst.Name) {
        Set-Item -Path "Function:script:$($kafkaFnAst.Name)" -Value $kafkaFnAst.Body.GetScriptBlock()
    }
}
foreach ($kafkaFnName in $kafkaTopologyFunctionNames) {
    if (-not (Test-Path "Function:$kafkaFnName")) { throw "k8s_deploy.ps1 缺少函数 $kafkaFnName(Kafka 拓扑计算被删了?后续断言无意义)" }
}

Test-Case 'Kafka 拓扑:单 broker 与改造前逐字一致,≥3 个 broker 时选举组固定三票、副本数 3、min.insync 2' {
    $one = Get-KafkaTopologyFor -Brokers 1 -Namespace 'ns'
    Assert-Equal -Expected '1@kafka-0.kafka-headless.ns.svc.cluster.local:9093' -Actual $one.ControllerQuorumVoters -Because '单 broker 的成员表不能变:PVC 里的元数据日志是按它 format 的'
    Assert-Equal -Expected '1/1/1/1/OrderedReady' -Actual ('{0}/{1}/{2}/{3}/{4}' -f $one.ControllerCount, $one.ReplicationFactor, $one.MinInsyncReplicas, $one.PdbMinAvailable, $one.PodManagementPolicy) -Because 'OrderedReady 是缺省值,既有单 broker StatefulSet 的不可变字段不能被改到'

    $three = Get-KafkaTopologyFor -Brokers 3 -Namespace 'ns'
    Assert-Equal -Expected '1@kafka-0.kafka-headless.ns.svc.cluster.local:9093,2@kafka-1.kafka-headless.ns.svc.cluster.local:9093,3@kafka-2.kafka-headless.ns.svc.cluster.local:9093' -Actual $three.ControllerQuorumVoters -Because 'node.id = 序号 + 1,三票'
    Assert-Equal -Expected '3/3/2/2/Parallel' -Actual ('{0}/{1}/{2}/{3}/{4}' -f $three.ControllerCount, $three.ReplicationFactor, $three.MinInsyncReplicas, $three.PdbMinAvailable, $three.PodManagementPolicy) -Because '三份副本允许一份掉队;OrderedReady 会让选举组与 Pod 就绪互相等'

    $five = Get-KafkaTopologyFor -Brokers 5 -Namespace 'ns'
    Assert-Equal -Expected $three.ControllerQuorumVoters -Actual $five.ControllerQuorumVoters -Because '加 broker 不能改选举组:成员表变了就不是加副本,而是迁移选举组'
    Assert-Equal -Expected '3/3/2/4' -Actual ('{0}/{1}/{2}/{3}' -f $five.ControllerCount, $five.ReplicationFactor, $five.MinInsyncReplicas, $five.PdbMinAvailable) -Because '第 4 个起只当 broker;PDB 仍是一次最多驱逐一个'

    foreach ($bad in @(0, 2, -1)) {
        $thrown = ''
        try { Get-KafkaTopologyFor -Brokers $bad -Namespace 'ns' | Out-Null } catch { $thrown = $_.Exception.Message }
        Assert-Match -Text $thrown -Pattern '1 或 ≥3' -Because "broker 数 $bad 必须被拒:两票的选举组挂一个就没有多数派"
    }
}

Test-Case 'Kafka 拓扑:缺省 broker 数跟档位走(非 dev 发布档或非 custom 运维档 = 3),显式值优先' {
    $script:InfraNamespace = 'ns'
    foreach ($case in @(
        @{ Brokers = 0; Release = 'dev';     Ops = 'custom';        Want = 1 }
        @{ Brokers = 0; Release = 'staging'; Ops = 'custom';        Want = 3 }
        @{ Brokers = 0; Release = 'prod';    Ops = 'custom';        Want = 3 }
        @{ Brokers = 0; Release = 'dev';     Ops = 'managed-cloud'; Want = 3 }
        @{ Brokers = 0; Release = 'dev';     Ops = 'bare-metal';    Want = 3 }
        @{ Brokers = 1; Release = 'prod';    Ops = 'managed-cloud'; Want = 1 }
        @{ Brokers = 5; Release = 'dev';     Ops = 'custom';        Want = 5 }
    )) {
        $script:KafkaBrokers = $case.Brokers
        $script:ReleaseProfile = $case.Release
        $script:OpsProfile = $case.Ops
        Assert-Equal -Expected $case.Want -Actual (Get-KafkaTopology).Brokers -Because "KafkaBrokers=$($case.Brokers) release=$($case.Release) ops=$($case.Ops)"
    }
}

Test-Case 'Kafka broker 数变更:只放行「不变」与「已是多 broker 再加」,其余一律拒绝' {
    foreach ($case in @(
        @{ Live = $null; Want = 1; Verdict = 'absent' }
        @{ Live = $null; Want = 3; Verdict = 'absent' }
        @{ Live = 1; Want = 1; Verdict = 'unchanged' }
        @{ Live = 3; Want = 3; Verdict = 'unchanged' }
        @{ Live = 3; Want = 5; Verdict = 'scale-up' }
        @{ Live = 1; Want = 3; Verdict = 'refuse' }
        @{ Live = 3; Want = 1; Verdict = 'refuse' }
        @{ Live = 5; Want = 3; Verdict = 'refuse' }
        @{ Live = 0; Want = 1; Verdict = 'refuse' }
        @{ Live = 2; Want = 3; Verdict = 'refuse' }
    )) {
        $liveText = if ($null -eq $case.Live) { '<不存在>' } else { [string]$case.Live }
        Assert-Equal -Expected $case.Verdict -Actual (Get-KafkaTopologyChangeVerdict -LiveReplicas $case.Live -DesiredBrokers $case.Want) -Because "线上 $liveText → 目标 $($case.Want):1 ↔ ≥3 会改选举组成员表,缩容会摘掉仍持有副本的 broker"
    }
}

Test-Case 'dev 档 Kafka 清单仍是单 broker:副本数 1,占位全部替换' {
    $kafka = Select-ManifestByName -Output $battleInfraRun.Output -Name 'kafka'
    Assert-Match -Text $kafka -Pattern '(?m)^  replicas: 1\s*$' -Because 'dev 档默认单 broker'
    Assert-Match -Text $kafka -Pattern 'podManagementPolicy: OrderedReady' -Because '单 broker 必须渲染成缺省值,否则既有 StatefulSet apply 时不可变字段冲突'
    Assert-Match -Text $kafka -Pattern 'name: KAFKA_DEFAULT_REPLICATION_FACTOR\s+value: "1"' -Because '单 broker 只能有一份副本'
    Assert-Match -Text $kafka -Pattern 'name: KAFKA_MIN_INSYNC_REPLICAS\s+value: "1"' -Because '单 broker 的 min.insync 只能是 1'
    Assert-Match -Text $kafka -Pattern 'minAvailable: 1' -Because '单 broker 不允许自愿驱逐'
    Assert-NotMatch -Text $kafka -Pattern 'name: KAFKA_NODE_ID|name: KAFKA_ADVERTISED_LISTENERS' -Because '每个 Pod 不同的身份由启动脚本按序号导出,模板里写死就全体同号'
    $topicJob = Select-ManifestByName -Output $battleInfraRun.Output -Name 'kafka-topic-init'
    Assert-Match -Text $topicJob -Pattern 'name: REPLICATION_FACTOR\s+value: "1"' -Because '预建 topic 的副本数与 broker 数同源'
}

Test-Case '-KafkaBrokers 3:StatefulSet / 选举组 / 副本数 / PDB / 预建 Job 一起变' {
    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-KafkaBrokers', '3')) -Env $BattleTestEnv
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '三 broker 的 DryRun 必须成功'
    Assert-Match -Text $run.Output -Pattern 'Kafka topology: brokers=3 controllers=3 replication_factor=3 min_insync_replicas=2' -Because '解析出的拓扑必须打出来,zone-up 与 infra-up 档位不一致时靠这行发现'
    $kafka = Select-ManifestByName -Output $run.Output -Name 'kafka'
    Assert-Match -Text $kafka -Pattern '(?m)^  replicas: 3\s*$' -Because '三个 broker'
    Assert-Match -Text $kafka -Pattern 'podManagementPolicy: Parallel' -Because 'OrderedReady 下 kafka-0 等多数派、kafka-1 等 kafka-0 Ready,互相等'
    Assert-Match -Text $kafka -Pattern 'name: KAFKA_CONTROLLER_QUORUM_VOTERS\s+value: "1@kafka-0\.kafka-headless\.contract-battle-infra\.svc\.cluster\.local:9093,2@kafka-1\.kafka-headless\.contract-battle-infra\.svc\.cluster\.local:9093,3@kafka-2\.kafka-headless\.contract-battle-infra\.svc\.cluster\.local:9093"' -Because '三票选举组,地址走发布未就绪地址的 headless'
    Assert-Match -Text $kafka -Pattern 'name: KAFKA_CONTROLLER_COUNT\s+value: "3"' -Because '启动脚本靠它决定谁兼任 controller、广播哪种地址'
    foreach ($name in @('KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR', 'KAFKA_TRANSACTION_STATE_LOG_REPLICATION_FACTOR', 'KAFKA_DEFAULT_REPLICATION_FACTOR')) {
        Assert-Match -Text $kafka -Pattern ("name: $name\s+value: `"3`"") -Because "${name}:消费位点等内部 topic 只有一份时,倒一个 broker 照样全体消费者停摆"
    }
    Assert-Match -Text $kafka -Pattern 'name: KAFKA_MIN_INSYNC_REPLICAS\s+value: "2"' -Because '三份副本允许一份掉队,不允许只剩一份还照常收写'
    Assert-Match -Text $kafka -Pattern 'minAvailable: 2' -Because '一次最多自愿驱逐一个 broker'
    Assert-Match -Text $kafka -Pattern 'PLAINTEXT://\$\{POD_NAME\}\.kafka-headless\.contract-battle-infra\.svc\.cluster\.local:9092' -Because '多 broker 必须每个 Pod 广播自己的 DNS'
    # (?-i):占位是全大写。断言助手默认不分大小写,而 DryRun 输出里日志 sidecar 的 Alloy 配置有内置标签 __path__,
    # 不关掉大小写无关就会把它误判成未替换的占位(2026-10-08 实跑发现)。
    Assert-NotMatch -Text $run.Output -Pattern '(?-i)__[A-Z][A-Z0-9_]*__' -Because '占位必须全部替换'
    $topicJob = Select-ManifestByName -Output $run.Output -Name 'kafka-topic-init'
    Assert-Match -Text $topicJob -Pattern 'name: REPLICATION_FACTOR\s+value: "3"' -Because '命令 topic 与审计 topic 必须按三份建'
}

Test-Case '负向:-KafkaBrokers 2 在任何清单生成之前被拒' {
    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-KafkaBrokers', '2')) -Env $BattleTestEnv
    Assert-True -Condition ($run.ExitCode -ne 0) -Because '两个 broker 的选举组没有容错,不能部署'
    Assert-Match -Text $run.Output -Pattern '1 或 ≥3' -Because '错误必须说明原因,而不是别的执行错误'
    Assert-NotMatch -Text $run.Output -Pattern 'BEGIN MANIFEST' -Because '参数非法时不能已经开始输出清单'
}

# ─────────────────────────────────────────────────────────────────
# MySQL 连接总账(G2b:docs/handoff/launch-readiness-20261013/spec_G2b.md §2、§6.1)
# ─────────────────────────────────────────────────────────────────
# 所有 zone 的 go/db、data_service、Java gateway 与全局的 friend、trade 共用同一台 MySQL(deploy/k8s/manifests/infra/mysql.yaml)。
# 这笔账只有一份,在 k8s_deploy.ps1:Get-MysqlConnectionPlan(规划档)、Get-MysqlConnectionLedgerInputs(从权威文件读池上限 /
# 副本数 / max_connections)、Get-MysqlConnectionLedger(纯函数:稳态 / 滚动余量 / 峰值 / 余量)。这里按 AST 把它们连同依赖抽出来
# 直接调用(与上面 deadline 守卫、Kafka 拓扑同一做法),不另抄一份公式。读文件的那几个经 $RepoRoot 定位仓库,由调用方设好。
# 写路径入口的 Write-MysqlConnectionLedger 不在名单里:它依赖脚本参数(-Command / -ZoneId / -ZonesConfigPath),用 DryRun 输出核对。
# 连接池镜像门禁 Get-MysqlPoolMirror / Assert-MysqlPoolMirror 也在名单里:它们只经 Get-AuthoritativeScalar 与 $RepoRoot 读三份服务配置,
# 负向用例把 $RepoRoot 指向临时目录里的配置副本。
$mysqlLedgerFunctionNames = @(
    'Get-AuthoritativeScalar', 'Get-MysqlConnectionPlan', 'Get-MysqlMaxConnectionsFromManifest',
    'Get-ManifestDeploymentReplicas', 'Get-MysqlConnectionLedgerInputs', 'Get-MysqlConnectionLedger',
    'Get-MysqlPoolMirror', 'Assert-MysqlPoolMirror'
)
foreach ($mysqlLedgerFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($mysqlLedgerFunctionNames -contains $mysqlLedgerFnAst.Name) {
        Set-Item -Path "Function:script:$($mysqlLedgerFnAst.Name)" -Value $mysqlLedgerFnAst.Body.GetScriptBlock()
    }
}
foreach ($mysqlLedgerFnName in $mysqlLedgerFunctionNames) {
    if (-not (Test-Path "Function:$mysqlLedgerFnName")) { throw "k8s_deploy.ps1 缺少函数 $mysqlLedgerFnName(MySQL 连接总账被删了?后续断言无意义)" }
}

# 用仓库权威文件的输入算一份总账。Z / E_z 由调用方给,其余形态参数(E_n / route store 是否 db_first / 滚动余量 / 运维余量)取规划档。
function Get-RepoMysqlConnectionLedger {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Plan,
        [Parameter(Mandatory = $true)][int]$ZoneCount,
        [Parameter(Mandatory = $true)][int]$ExtraZoneStores
    )
    # Get-MysqlConnectionLedgerInputs 及其依赖按动态作用域读这个变量(在 k8s_deploy.ps1 里它是脚本级变量)。
    $RepoRoot = Get-RepoRoot
    return Get-MysqlConnectionLedger -Inputs (Get-MysqlConnectionLedgerInputs) -ZoneCount $ZoneCount -ExtraZoneStores $ExtraZoneStores `
        -NonZoneStores $Plan.NonZoneStores -RouteStoreDbFirst $Plan.RouteStoreDbFirst `
        -RolloutExtraPodsPerDeployment $Plan.RolloutExtraPodsPerDeployment -OpsReserve $Plan.OpsReserve
}

# 把一份总账拍成一行 "键=值 键=值 …",断言失败时看分项用。
function Format-MysqlConnectionLedger {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Ledger)
    return (@($Ledger.Keys | ForEach-Object { "$_=$($Ledger[$_])" }) -join ' ')
}

# DryRun 输出里「连这台 MySQL 的 ConfigMap」的名字(去重、排序)。一个 manifest 块可能是多文档(--- 分隔),逐文档看;
# 只看非注释行:模板注释里提到 MySQL 地址不算连它。地址认两种形状:Go 服务的 Host / Hosts "mysql.<ns>:3306"(与 MySQL 同
# namespace 时可以省掉 .<ns>,一并认)与 JDBC url。命中的文档读不出 metadata.name 就 throw(fail-closed:不能因为名字没读到而漏算一个)。
function Get-MysqlClientConfigMapNames {
    param([Parameter(Mandatory = $true)][string]$Output)

    $names = New-Object System.Collections.Generic.List[string]
    foreach ($block in (Get-ManifestBlocks -Output $Output)) {
        foreach ($doc in ($block -split '(?m)^---[ \t]*\r?$')) {
            if ($doc -notmatch '(?m)^kind:[ \t]*ConfigMap\s*$') { continue }
            $effective = @($doc -split "`n" | Where-Object { $_ -notmatch '^\s*#' }) -join "`n"
            if ($effective -notmatch 'mysql(?:\.[^\s:/]+)?:3306|jdbc:mysql://') { continue }
            # name 紧跟在 metadata: 之下(中间只允许注释行);生成器的 ConfigMap 都是这个形状。
            $name = [regex]::Match($doc, '(?m)^metadata:[ \t]*\r?\n(?:[ \t]*#[^\n]*\n)*[ ]{2}name:[ \t]*"?(?<name>[^\s"]+)"?\s*$')
            if (-not $name.Success) { throw "DryRun 输出里有一份连 MySQL 的 ConfigMap 读不出 metadata.name(形状变了?),无法核对它是否在连接总账里" }
            $names.Add($name.Groups['name'].Value)
        }
    }
    return @($names | Sort-Object -Unique)
}

Test-Case 'MySQL 连接总账:规划档峰值 ≤ mysql.yaml 的 max_connections' {
    # max_connections 是按规划档的峰值取的(mysql.yaml「连接上限」注释)。调大任何一个池、加副本、改规划档而不同步上限,
    # 都在这里点名失败,而不是等集群里出 1040。
    $plan = Get-MysqlConnectionPlan
    $ledger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $plan.ZoneCount -ExtraZoneStores $plan.ExtraZoneStores
    Assert-True -Condition ($ledger.Peak -le $ledger.MaxConnections) -Because ("规划档(zones={0},每个 go/db 实例打开 {1} 个额外 zone 库)的峰值 {2} 超过 max_connections={3}:要么收紧池 / 规划档,要么同步抬高 deploy/k8s/manifests/infra/mysql.yaml 的 max_connections 与内存 limits。分项:{4}" -f $plan.ZoneCount, $plan.ExtraZoneStores, $ledger.Peak, $ledger.MaxConnections, (Format-MysqlConnectionLedger -Ledger $ledger))
}

Test-Case 'MySQL 连接总账:deploy/k8s/zones.json 的 zone 数(E_z=Z−1)峰值 ≤ max_connections' {
    # 仓库自带的 zones 配置(all-up 的默认输入)也必须装得下:Z 取 zones.json 的 zone 数,每个 go/db 实例按最坏情形打开其余
    # 全部 zone 的库(E_z = Z − 1),其余形态参数取规划档。往 zones.json 加 zone 加到超限时,这里先于部署失败。
    $zoneCount = @((Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.json') -Raw | ConvertFrom-Json).zones).Count
    Assert-True -Condition ($zoneCount -gt 0) -Because "deploy/k8s/zones.json 里应当至少有一个 zone(本断言的前提),实际 $zoneCount 个"
    $plan = Get-MysqlConnectionPlan
    $ledger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $zoneCount -ExtraZoneStores ($zoneCount - 1)
    Assert-True -Condition ($ledger.Peak -le $ledger.MaxConnections) -Because ("deploy/k8s/zones.json 的 {0} 个 zone(每个 go/db 实例打开其余 {1} 个 zone 的库)峰值 {2} 超过 max_connections={3}。分项:{4}" -f $zoneCount, ($zoneCount - 1), $ledger.Peak, $ledger.MaxConnections, (Format-MysqlConnectionLedger -Ledger $ledger))
}

Test-Case '自检:总账公式复现 2026-10-12 核算(Z=4 → 920、Z=2 → 500),对旧 max_connections=500 判超限,输入非法即 throw' {
    # 合成输入 = 2026-10-12 手算那一版的口径(docs/PROGRESS.md「落点多库连接拓扑核算」):gateway 还是每副本 10 条,
    # max_connections 还是 500。不读仓库文件:本用例验的是公式本身,不该随服务配置的调整变红。每次取一份新的,下面有几处会改它。
    $newLegacyInputs = {
        [ordered]@{
            MaxConnections = 500
            DbReplicas = 2; DbHomeMaxOpen = 60; DbZoneStoreMaxOpen = 8; DbNonZoneStoreMaxOpen = 8
            DsReplicas = 2; DsAuditStorePools = 3; DsAuditMaxOpen = 5; DsPlayerNameMaxOpen = 20; DsRouteMaxOpen = 20
            GatewayReplicas = 2; GatewayMaxPool = 10
            FriendReplicas = 2; FriendMaxOpen = 20; TradeReplicas = 2; TradeMaxOpen = 20
        }
    }

    # 当时的名义值:不算额外落点库(E_z = E_n = 0)、route store 不计(F = 0)、不含滚动余量与运维余量。
    # 每 zone = 2 × (db 60 + ds 35 + gateway 10) = 210;全局 = friend 2 × 20 + trade 2 × 20 = 80。
    $nominalShape = @{ ExtraZoneStores = 0; NonZoneStores = 0; RouteStoreDbFirst = $false; RolloutExtraPodsPerDeployment = 0; OpsReserve = 0 }
    foreach ($case in @(@{ Zones = 4; Steady = 920 }, @{ Zones = 2; Steady = 500 })) {
        $ledger = Get-MysqlConnectionLedger -Inputs (& $newLegacyInputs) -ZoneCount $case.Zones @nominalShape
        Assert-Equal -Expected '60/35/10/210/80' -Actual ('{0}/{1}/{2}/{3}/{4}' -f $ledger.DbPerInstance, $ledger.DsPerInstance, $ledger.GatewayPerInstance, $ledger.PerZone, $ledger.Global) -Because "Z=$($case.Zones):db / ds / gateway 每实例、每 zone、全局 五项"
        Assert-Equal -Expected $case.Steady -Actual $ledger.Steady -Because "Z=$($case.Zones) 的稳态必须复现 2026-10-12 的核算"
        Assert-Equal -Expected $case.Steady -Actual $ledger.Peak -Because '不含滚动余量与运维余量时,峰值 = 稳态'
        Assert-Equal -Expected (500 - $case.Steady) -Actual $ledger.Headroom -Because '余量 = max_connections − 峰值(Z=2 恰好为 0,Z=4 为负)'
    }

    # 旧上限 500 对规划档必须判超限(峰值 > max_connections,余量为负)—— 这就是上限要跟着规划档走的原因。
    $plan = Get-MysqlConnectionPlan
    $planShape = @{
        ZoneCount = $plan.ZoneCount; ExtraZoneStores = $plan.ExtraZoneStores; NonZoneStores = $plan.NonZoneStores
        RouteStoreDbFirst = $plan.RouteStoreDbFirst; RolloutExtraPodsPerDeployment = $plan.RolloutExtraPodsPerDeployment; OpsReserve = $plan.OpsReserve
    }
    $overLimit = Get-MysqlConnectionLedger -Inputs (& $newLegacyInputs) @planShape
    Assert-True -Condition ($overLimit.Peak -gt 500 -and $overLimit.Headroom -lt 0) -Because ("规划档对旧上限 500 必须判超限,实际:{0}" -f (Format-MysqlConnectionLedger -Ledger $overLimit))

    # 滚动口径:同一份输入,R=2 比 R=0 恰好多出 2 × 每实例之和(db 60 + ds 35 + gateway 10 + friend 20 + trade 20 = 145)。
    # 多出来的是「实例」(maxSurge 的新 Pod + 仍在优雅退出、连接还没释放的旧 Pod),不乘副本数,也不乘 zone 数。
    $rolloutShape = @{ ZoneCount = 4; ExtraZoneStores = 0; NonZoneStores = 0; RouteStoreDbFirst = $false; OpsReserve = 0 }
    $withoutRollout = Get-MysqlConnectionLedger -Inputs (& $newLegacyInputs) -RolloutExtraPodsPerDeployment 0 @rolloutShape
    $withRollout = Get-MysqlConnectionLedger -Inputs (& $newLegacyInputs) -RolloutExtraPodsPerDeployment 2 @rolloutShape
    Assert-Equal -Expected (2 * 145) -Actual ($withRollout.Peak - $withoutRollout.Peak) -Because 'R=2 与 R=0 的峰值之差必须是 2 × (db + ds + gateway + friend + trade 每实例之和)'
    Assert-Equal -Expected (2 * 145) -Actual $withRollout.RolloutSurge -Because '这个差值就是分项里的 RolloutSurge'
    Assert-Equal -Expected $withoutRollout.Steady -Actual $withRollout.Steady -Because '滚动余量不进稳态'

    # 逐项钉公式:副本数、池上限、形态参数各取互不相同的值,任何一项漏算、两个额外库旋钮接反、副本数错配,结果都对不上。
    #   db 每实例 = 60 + 2 × 7(zone 家族额外库)+ 1 × 11(非 zone 落点库)= 85
    #   ds 每实例 = 3 × 5 + 20 + 30(route store 按 db_first 计入)= 65
    #   每 zone   = 2 × 85 + 3 × 65 + 4 × 10 = 405;全局 = 5 × 20 + 6 × 40 = 340;稳态 = 3 × 405 + 340 = 1555
    #   滚动余量  = 1 × (85 + 65 + 10 + 20 + 40) = 220;峰值 = 1555 + 220 + 7 = 1782;余量 = 5000 − 1782 = 3218
    $distinctInputs = [ordered]@{
        MaxConnections = 5000
        DbReplicas = 2; DbHomeMaxOpen = 60; DbZoneStoreMaxOpen = 7; DbNonZoneStoreMaxOpen = 11
        DsReplicas = 3; DsAuditStorePools = 3; DsAuditMaxOpen = 5; DsPlayerNameMaxOpen = 20; DsRouteMaxOpen = 30
        GatewayReplicas = 4; GatewayMaxPool = 10
        FriendReplicas = 5; FriendMaxOpen = 20; TradeReplicas = 6; TradeMaxOpen = 40
    }
    $distinct = Get-MysqlConnectionLedger -Inputs $distinctInputs -ZoneCount 3 -ExtraZoneStores 2 -NonZoneStores 1 -RouteStoreDbFirst $true -RolloutExtraPodsPerDeployment 1 -OpsReserve 7
    Assert-Equal -Expected '85/65/10/405/340/1555/220/7/1782/5000/3218' -Actual ('{0}/{1}/{2}/{3}/{4}/{5}/{6}/{7}/{8}/{9}/{10}' -f $distinct.DbPerInstance, $distinct.DsPerInstance, $distinct.GatewayPerInstance, $distinct.PerZone, $distinct.Global, $distinct.Steady, $distinct.RolloutSurge, $distinct.OpsReserve, $distinct.Peak, $distinct.MaxConnections, $distinct.Headroom) -Because '分项依次是 db / ds / gateway 每实例、每 zone、全局、稳态、滚动余量、运维余量、峰值、上限、余量'

    # 输入非法一律 throw 并点名是哪个键(fail-closed:不能拿一个猜出来的连接数去和上限比)。
    $missingKey = & $newLegacyInputs
    $missingKey.Remove('GatewayMaxPool')
    $zeroValue = & $newLegacyInputs
    $zeroValue['DbHomeMaxOpen'] = 0
    $notNumber = & $newLegacyInputs
    $notNumber['DsAuditMaxOpen'] = '5x'
    foreach ($bad in @(
        @{ Why = '缺键'; Key = 'GatewayMaxPool'; Inputs = $missingKey }
        @{ Why = '值为 0'; Key = 'DbHomeMaxOpen'; Inputs = $zeroValue }
        @{ Why = '值不是数字'; Key = 'DsAuditMaxOpen'; Inputs = $notNumber }
    )) {
        $message = ''
        try { Get-MysqlConnectionLedger -Inputs $bad.Inputs -ZoneCount 4 @nominalShape | Out-Null } catch { $message = $_.Exception.Message }
        Assert-Match -Text $message -Pattern ('MySQL 连接总账.*输入 {0}=' -f $bad.Key) -Because "$($bad.Why) 必须 throw 并点名 $($bad.Key),不能当成 0 条或某个默认值算下去"
    }

    # 形态参数越界同样 throw:滚动期间多出来的实例数不能是负数。
    $negativeRollout = @{} + $nominalShape
    $negativeRollout['RolloutExtraPodsPerDeployment'] = -1
    $message = ''
    try { Get-MysqlConnectionLedger -Inputs (& $newLegacyInputs) -ZoneCount 4 @negativeRollout | Out-Null } catch { $message = $_.Exception.Message }
    Assert-Match -Text $message -Pattern 'RolloutExtraPodsPerDeployment=-1\b.*不能为负' -Because 'R 传 -1 必须 throw 并点名参数与值,不能把滚动余量算成负数去抵稳态'
}

Test-Case '连 MySQL 的 ConfigMap 只有总账认识的那些(zone-up 三个、infra-up 两个)' {
    # 总账(Get-MysqlConnectionLedgerInputs)按服务逐个登记池上限。新增一个连这台 MySQL 的服务而不登记,它占的连接就不在账上,
    # 上面「峰值 ≤ max_connections」照样通过。这里把「谁在连」钉成名单:名单变了,先去总账加一项,再来改这里。
    $zoneExpected = @('go-svc-db-config', 'go-svc-data-service-config', 'java-svc-gateway-config' | Sort-Object) -join ','
    Assert-Equal -Expected $zoneExpected -Actual (@(Get-MysqlClientConfigMapNames -Output $devOut) -join ',') -Because 'zone-up 产物里连 MySQL 的 ConfigMap 必须恰好是 db / data-service / gateway 三个(总账的每 zone 部分只算了它们)'

    # 全局服务(friend / trade)只在 infra-up / all-up 的基础设施阶段生成(Apply-GlobalGoSvcManifests),zone-up 的产物里没有:
    # 只扫 zone-up 的话,新增一个连 MySQL 的全局服务能绕过总账而无人察觉。本文件其余 infra-up DryRun 都带 -SkipGoSvc,所以单独起一次:
    # 去掉 -SkipGoSvc、补上 -GoSvcRegistry;同时去掉 -WaitReady,免得 dev 档在 DryRun 里去走「等 trade-migrate / friend-migrate」那一段。
    $infraArgs = @($BattleInfraArgs | Where-Object { $_ -notin @('-SkipGoSvc', '-WaitReady') }) + @('-GoSvcRegistry', 'registry.invalid/test')
    Assert-True -Condition (($infraArgs -contains '-DryRun') -and -not ($infraArgs -contains '-SkipGoSvc')) -Because '夹具前提:这次 infra-up 必须仍是 DryRun(不碰任何集群),且不再跳过 Go 服务'
    $infraRun = Invoke-DeployDryRun -Arguments $infraArgs -Env $BattleTestEnv
    Assert-Equal -Expected 0 -Actual $infraRun.ExitCode -Because "带全局 Go 服务的 infra-up DryRun 必须成功。输出: $($infraRun.Output)"
    $infraExpected = @('go-svc-friend-config', 'go-svc-trade-config' | Sort-Object) -join ','
    Assert-Equal -Expected $infraExpected -Actual (@(Get-MysqlClientConfigMapNames -Output $infraRun.Output) -join ',') -Because 'infra-up 产物里连 MySQL 的 ConfigMap 必须恰好是 friend / trade 两个(总账的全局部分只算了它们)'
    $friendConfig = Select-ManifestByName -Output $infraRun.Output -Name 'go-svc-friend-config'
    Assert-Match -Text $friendConfig -Pattern 'RequireTiDB: false' -Because '开发 ConfigMap 必须显式说明未要求 TiDB'
}

Test-Case '部署写路径打印 MySQL connection ledger(zone-up DryRun)' {
    # 部署的人靠这一行看「这次部署之后连接预算还剩多少」。行里的数字用 AST 抽出的同一组函数重算后比对,不抄数字:
    # 调池 / 调规划档不需要来改本用例;打印的那份与契约测试钉住的那份分了家,才会在这里失败。
    $plan = Get-MysqlConnectionPlan
    $planLedger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $plan.ZoneCount -ExtraZoneStores $plan.ExtraZoneStores
    # 基线 DryRun($BaseArgs)不带 -ZonesConfigPath,读的是 deploy/k8s/zones.json;Z = 其中的全部 zone ∪ 本次 -ZoneId(基线是 101)。
    # 现状:zones.json 是 101、102 两个 zone,101 已在其中,所以 Z = 2。E_z 取 Z − 1,其余形态参数取规划档。
    $zoneIds = @(@((Get-Content -LiteralPath (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.json') -Raw | ConvertFrom-Json).zones | ForEach-Object { [int]$_.zoneId }) + @(101) | Sort-Object -Unique)
    $deployLedger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $zoneIds.Count -ExtraZoneStores ($zoneIds.Count - 1)

    $ledgerLines = @($devOut -split "`r?`n" | Where-Object { $_ -match '^\s*MySQL connection ledger: zones=' })
    Assert-Equal -Expected 1 -Actual $ledgerLines.Count -Because 'zone-up 写路径入口必须恰好打印一行 MySQL connection ledger(Write-MysqlConnectionLedger)'
    $ledgerLine = [string]$ledgerLines[0]
    $deployPart = 'MySQL connection ledger: zones={0} steady={1} peak={2} max_connections={3} headroom={4};' -f $zoneIds.Count, $deployLedger.Steady, $deployLedger.Peak, $deployLedger.MaxConnections, $deployLedger.Headroom
    $planPart = '; plan zones={0} peak={1} headroom={2}' -f $plan.ZoneCount, $planLedger.Peak, $planLedger.Headroom
    Assert-Match -Text $ledgerLine -Pattern ('^\s*' + [regex]::Escape($deployPart)) -Because '本次部署那一份(zone 数 / 稳态 / 峰值 / 上限 / 余量)必须与总账函数按同一组输入算出的值一致'
    # 行尾锚定:zones 配置在场时不该带 zones_config=missing 的尾巴。
    Assert-Match -Text $ledgerLine -Pattern ([regex]::Escape($planPart) + '\s*$') -Because '规划档那一份(zone 数 / 峰值 / 余量)必须与 Get-MysqlConnectionPlan 算出的值一致,且行尾没有 zones_config=missing'

    $firstKubectlAt = $devOut.IndexOf('[dry-run] kubectl')
    Assert-True -Condition ($firstKubectlAt -lt 0 -or $devOut.IndexOf('MySQL connection ledger:') -lt $firstKubectlAt) -Because '总账在写路径入口打印,先于任何 kubectl 写操作'
}

Test-Case '负向:zones.10zones.yaml 时打 [MySQLConnectionLedger]OverLimit 警告且不阻断' {
    # 10 个 zone 的样例配置在单台 MySQL 上装不下(额外 zone 库的池随 zone 数平方增长)。超限只告警、不拒绝部署
    # (是否升级为部署门禁待拍板,见 Write-MysqlConnectionLedger 注释)。
    # -ZonesConfigPath 给绝对路径:子进程的工作目录不保证是仓库根(与上面 all-up 用例同一写法)。
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-ZonesConfigPath', (Join-Path (Get-RepoRoot) 'deploy/k8s/zones.10zones.yaml'), '-SkipGoSvc', '-SkipJavaSvc'))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "连接总账超限只告警、不阻断部署。输出: $($run.Output)"
    # 只断警告开头不含空格的那一段:Write-Warning 若被 ConsoleHost 按窗口宽度折行,折点在空格处(先例见下面 gateway 管理面口令的占位警告)。
    Assert-Match -Text $run.Output -Pattern '\[MySQLConnectionLedger\]OverLimit:' -Because '峰值超过 max_connections 时必须明说,不能只留一个负的 headroom 让人自己看'
    # $BaseArgs 是 zone-up:Z = 配置里的 10 个 zone ∪ 本次 -ZoneId 101(不在配置里也并入:这个 zone 确实要部署)= 11。
    Assert-Match -Text $run.Output -Pattern 'MySQL connection ledger: zones=11 steady=\d+ peak=\d+ max_connections=\d+ headroom=-\d+;' -Because '本次部署那一份按 11 个 zone 算,余量为负'
    Assert-Match -Text $run.Output -Pattern '--- BEGIN MANIFEST ---' -Because '不阻断:告警之后清单照常生成'
    Assert-NotMatch -Text $devOut -Pattern '\[MySQLConnectionLedger\]OverLimit:' -Because '对照:仓库 zones.json 与规划档都在上限内,基线 DryRun 不该打这条警告(否则本条区分不出「超限才告警」)'
}

# 下面三例共用 zones.10zones.yaml 及其 zoneId。本套测试不带 YAML 解析器:按 Get-ZonesFromJson 的 YAML 回退解析的写法
# (键独占一行、行尾不带注释)读 zone 级 zoneId 行。-ZonesConfigPath 给绝对路径,理由同上。
# 文件读不到时得到空列表,由各例的夹具前提点名失败,不在脚本顶层 throw 掉整套测试。
$tenZonesConfigPath = Join-Path (Get-RepoRoot) 'deploy/k8s/zones.10zones.yaml'
$tenZonesRaw = [string](Get-Content -LiteralPath $tenZonesConfigPath -Raw -ErrorAction SilentlyContinue)
$tenZoneIds = @([regex]::Matches($tenZonesRaw, '(?m)^[ \t]+zoneId:[ \t]*(\d+)[ \t]*\r?$') | ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique)

Test-Case 'all-up(zones.10zones.yaml):总账只数 zones 配置,zones=10 不并入 -ZoneId,峰值仍超限并打 [MySQLConnectionLedger]OverLimit' {
    # all-up / infra-up 不为 -ZoneId 部署 zone 级工作负载(db / data-service / gateway):-ZoneId 在这两条命令里用作全局 Go 服务与
    # battle 的 ZoneId、mysql-init-sql 预建库的输入,本身不产生每 zone 的 MySQL 连接,不论缺省值 101 还是显式传入都不并入。
    # 以前按 Get-InfraZoneIds 的口径把它并进来,zones 配置不含它时 Z 凭空多 1,可能打出虚假的 OverLimit
    # (见 Write-MysqlConnectionLedger 注释)。这里显式传一个不在配置里的 -ZoneId(取缺省值 101):
    # all-up 不论缺省还是显式都不并入它;显式传是为了不依赖脚本缺省值 —— 缺省值哪天改成配置里已有的 zone,不传就区分不出新旧口径。
    # 只部署 C++ 节点:-SkipInfra(不装配 battle 全局池,免得还要 battle 密钥)、-SkipGoSvc、-SkipJavaSvc;dev 档不走发布预检。
    Assert-Equal -Expected 10 -Actual $tenZoneIds.Count -Because "夹具前提:zones.10zones.yaml 应当有 10 个互不相同的 zoneId(实际 $($tenZoneIds -join ','))"
    Assert-True -Condition ($tenZoneIds -notcontains 101) -Because '夹具前提:zones.10zones.yaml 不含 101,否则并不并入 -ZoneId 结果相同,本例区分不出新旧口径'
    $plan = Get-MysqlConnectionPlan
    $deployLedger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $tenZoneIds.Count -ExtraZoneStores ($tenZoneIds.Count - 1)
    Assert-True -Condition ($deployLedger.Peak -gt $deployLedger.MaxConnections) -Because ("夹具前提:10 个 zone 的峰值必须超过 max_connections,否则「仍打 OverLimit」无从谈起。分项:{0}" -f (Format-MysqlConnectionLedger -Ledger $deployLedger))

    $allUpArgs = @('-Command', 'all-up', '-DryRun', '-SkipInfra', '-SkipGoSvc', '-SkipJavaSvc', '-ZonesConfigPath', $tenZonesConfigPath, '-ZoneId', '101')
    Assert-True -Condition ($allUpArgs -contains '-DryRun') -Because '夹具前提:all-up 必须是 DryRun(不带它会对本机当前 kubectl context 真执行 apply)'
    $run = Invoke-DeployDryRun -Arguments $allUpArgs
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "连接总账超限只告警、不阻断 all-up。输出: $($run.Output)"
    $ledgerLines = @($run.Output -split "`r?`n" | Where-Object { $_ -match '^\s*MySQL connection ledger: zones=' })
    Assert-Equal -Expected 1 -Actual $ledgerLines.Count -Because 'all-up 写路径入口必须恰好打印一行 MySQL connection ledger'
    $deployPart = 'MySQL connection ledger: zones={0} steady={1} peak={2} max_connections={3} headroom={4};' -f $tenZoneIds.Count, $deployLedger.Steady, $deployLedger.Peak, $deployLedger.MaxConnections, $deployLedger.Headroom
    Assert-Match -Text ([string]$ledgerLines[0]) -Pattern ('^\s*' + [regex]::Escape($deployPart)) -Because 'all-up 本次部署那一份只数 zones 配置的 10 个 zone(E_z = 9),不是并入 -ZoneId 101 之后的 11 个'
    Assert-NotMatch -Text ([string]$ledgerLines[0]) -Pattern 'zones_config=missing' -Because 'zones 配置在场,行尾不该带缺席标记'
    # 只断警告开头不含空格的那一段(理由同上例)。
    Assert-Match -Text $run.Output -Pattern '\[MySQLConnectionLedger\]OverLimit:' -Because '10 个 zone 的峰值仍超过 max_connections,修口径不能顺手把真实超限的告警也去掉'
    Assert-Match -Text $run.Output -Pattern '--- BEGIN MANIFEST ---' -Because '不阻断:告警之后清单照常生成'
}

Test-Case '对照 zone-up(zones.10zones.yaml):-ZoneId 不在配置里也并入总账,zones=11' {
    # 与上例同一份 zones 配置,只换命令:zone-up 部署的就是 -ZoneId 那个 zone,不在配置里(例如新开的区还没登记)也要算进去。
    # 取 11 而不是缺省值 101:上面「负向:zones.10zones.yaml…」那例用的是 $BaseArgs 的 101(同样 zones=11),
    # 两例合起来说明并入的是本次 -ZoneId,不是认 101 这个值。
    $outsideZoneId = 11
    Assert-True -Condition ($tenZoneIds.Count -gt 0 -and $tenZoneIds -notcontains $outsideZoneId) -Because "夹具前提:zones.10zones.yaml 读得出 zoneId,且不含 $outsideZoneId(实际 $($tenZoneIds -join ','))"
    $plan = Get-MysqlConnectionPlan
    $zoneCount = $tenZoneIds.Count + 1
    $deployLedger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $zoneCount -ExtraZoneStores ($zoneCount - 1)

    $zoneUpArgs = @('-Command', 'zone-up', '-ZoneName', 'contract-test', '-ZoneId', [string]$outsideZoneId, '-DryRun', '-SkipGoSvc', '-SkipJavaSvc', '-ZonesConfigPath', $tenZonesConfigPath)
    $run = Invoke-DeployDryRun -Arguments $zoneUpArgs
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "连接总账超限只告警、不阻断 zone-up。输出: $($run.Output)"
    $ledgerLines = @($run.Output -split "`r?`n" | Where-Object { $_ -match '^\s*MySQL connection ledger: zones=' })
    Assert-Equal -Expected 1 -Actual $ledgerLines.Count -Because 'zone-up 写路径入口必须恰好打印一行 MySQL connection ledger'
    $deployPart = 'MySQL connection ledger: zones={0} steady={1} peak={2} max_connections={3} headroom={4};' -f $zoneCount, $deployLedger.Steady, $deployLedger.Peak, $deployLedger.MaxConnections, $deployLedger.Headroom
    Assert-Match -Text ([string]$ledgerLines[0]) -Pattern ('^\s*' + [regex]::Escape($deployPart)) -Because "zone-up 本次部署那一份 = zones 配置的 $($tenZoneIds.Count) 个 zone ∪ -ZoneId $outsideZoneId = $zoneCount 个(E_z = $($zoneCount - 1))"
}

Test-Case 'infra-up(zones.10zones.yaml):总账只数 zones 配置,zones=10 不并入 -ZoneId;预建库照旧并入 -ZoneId' {
    # 与 all-up 例同一口径,但走 infra-up 那一支(all-up 例带 -SkipInfra,覆盖不到这里)。现有的 $battleInfraRun 读默认 zones.json(101、102),
    # 而 -ZoneId 缺省也是 101,并不并入结果都是 zones=2,区分不出新旧口径;所以换成不含 101 的 zones.10zones.yaml,并显式传 -ZoneId 101
    # (理由同 all-up 例)。复用 $BattleInfraArgs / $BattleTestEnv:infra-up 要装配 battle 全局池,需要 battle / gate 票据密钥。
    Assert-Equal -Expected 10 -Actual $tenZoneIds.Count -Because "夹具前提:zones.10zones.yaml 应当有 10 个互不相同的 zoneId(实际 $($tenZoneIds -join ','))"
    Assert-True -Condition ($tenZoneIds -notcontains 101) -Because '夹具前提:zones.10zones.yaml 不含 101,否则并不并入 -ZoneId 结果相同,本例区分不出新旧口径'
    Assert-True -Condition (($BattleInfraArgs -contains '-DryRun') -and -not ($BattleInfraArgs -contains '-ZoneId') -and -not ($BattleInfraArgs -contains '-ZonesConfigPath')) -Because '夹具前提:infra-up 必须是 DryRun(不带它会对本机当前 kubectl context 真执行 apply),且 $BattleInfraArgs 自己不带 -ZoneId / -ZonesConfigPath(重复传参会让子进程直接失败)'
    $plan = Get-MysqlConnectionPlan
    $deployLedger = Get-RepoMysqlConnectionLedger -Plan $plan -ZoneCount $tenZoneIds.Count -ExtraZoneStores ($tenZoneIds.Count - 1)
    Assert-True -Condition ($deployLedger.Peak -gt $deployLedger.MaxConnections) -Because ("夹具前提:10 个 zone 的峰值必须超过 max_connections,否则「仍打 OverLimit」无从谈起。分项:{0}" -f (Format-MysqlConnectionLedger -Ledger $deployLedger))

    $run = Invoke-DeployDryRun -Arguments ($BattleInfraArgs + @('-ZonesConfigPath', $tenZonesConfigPath, '-ZoneId', '101')) -Env $BattleTestEnv
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "连接总账超限只告警、不阻断 infra-up。输出: $($run.Output)"
    $ledgerLines = @($run.Output -split "`r?`n" | Where-Object { $_ -match '^\s*MySQL connection ledger: zones=' })
    Assert-Equal -Expected 1 -Actual $ledgerLines.Count -Because 'infra-up 写路径入口必须恰好打印一行 MySQL connection ledger'
    $deployPart = 'MySQL connection ledger: zones={0} steady={1} peak={2} max_connections={3} headroom={4};' -f $tenZoneIds.Count, $deployLedger.Steady, $deployLedger.Peak, $deployLedger.MaxConnections, $deployLedger.Headroom
    Assert-Match -Text ([string]$ledgerLines[0]) -Pattern ('^\s*' + [regex]::Escape($deployPart)) -Because 'infra-up 本次部署那一份只数 zones 配置的 10 个 zone(E_z = 9),不是并入 -ZoneId 101 之后的 11 个'
    Assert-NotMatch -Text ([string]$ledgerLines[0]) -Pattern 'zones_config=missing' -Because 'zones 配置在场,行尾不该带缺席标记'
    # 只断警告开头不含空格的那一段(理由同上面「负向:zones.10zones.yaml…」例)。
    Assert-Match -Text $run.Output -Pattern '\[MySQLConnectionLedger\]OverLimit:' -Because '10 个 zone 的峰值仍超过 max_connections,修口径不能顺手把真实超限的告警也去掉'
    # 对照:mysql-init-sql 预建库的口径(Get-InfraZoneIds = zones 配置 ∪ -ZoneId)有意不同,照旧预建 -ZoneId 的库,留给之后的 zone-up -ZoneId。
    # 仓库静态的 deploy/mysql-init/00_init_zone_dbs.sql 只建 zone_1_db / zone_2_db,且 CREATE 里库名不带反引号,不会冒充这一行。
    $initSql = Select-ManifestByName -Output $run.Output -Name 'mysql-init-sql'
    Assert-Match -Text $initSql -Pattern ([regex]::Escape('CREATE DATABASE IF NOT EXISTS `zone_101_db`;')) -Because 'infra-up 的 mysql-init-sql 仍要预建 -ZoneId 101 的库:总账不并入 -ZoneId,不等于预建库也不并入'
}

Test-Case 'zones 配置缺席(zone-up DryRun):总账按 zones=1 算、行尾标 zones_config=missing,不打 Get-InfraZoneIds 的 WARNING' {
    # Write-MysqlConnectionLedger 的缺席分支(spec_G2b §5):Z 取 1(只有 -ZoneId),行尾带 zones_config=missing,且不调 Get-InfraZoneIds ——
    # 它那条 WARNING 讲的是「mysql-init-sql 只预建 -ZoneId 的库」,出现在 zone-up 的输出里是误导。
    # -ZonesConfigPath 给临时目录下一个不存在的绝对路径(只造路径、不建文件);zone-up 不在别处读 zones 配置。
    $missingZonesPath = Join-Path ([System.IO.Path]::GetTempPath()) ('zones-config-missing-' + [guid]::NewGuid().ToString('N') + '.json')
    Assert-True -Condition (-not (Test-Path -LiteralPath $missingZonesPath)) -Because '夹具前提:zones 配置路径必须不存在'
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-SkipGoSvc', '-SkipJavaSvc', '-ZonesConfigPath', $missingZonesPath))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "zones 配置缺席不该阻断 zone-up。输出: $($run.Output)"
    $ledgerLines = @($run.Output -split "`r?`n" | Where-Object { $_ -match '^\s*MySQL connection ledger: zones=' })
    Assert-Equal -Expected 1 -Actual $ledgerLines.Count -Because '写路径入口必须恰好打印一行 MySQL connection ledger'
    Assert-Match -Text ([string]$ledgerLines[0]) -Pattern 'MySQL connection ledger: zones=1 ' -Because 'zones 配置缺席时本次部署那一份只算 -ZoneId 一个 zone'
    Assert-Match -Text ([string]$ledgerLines[0]) -Pattern 'zones_config=missing\s*$' -Because '缺席要在行尾标出来,部署的人才知道这份账没算其它 zone'
    Assert-NotMatch -Text $run.Output -Pattern 'Zones configuration missing' -Because '缺席分支不调 Get-InfraZoneIds,不该出现它那条讲 mysql-init-sql 预建库的 WARNING'
}

# 连接池镜像门禁(Assert-MysqlPoolMirror)读的全部文件。夹具用例把它们拷到临时目录再逐项改坏,不碰仓库。
$mysqlPoolMirrorFixtureFiles = @(
    'go/db/etc/db.yaml'
    'go/data_service/etc/data_service.yaml'
    'java/gateway_node/src/main/resources/application.yaml'
)

# 夹具:把临时副本里某个键路径(与 release_common.ps1 ConvertFrom-YamlToFlatMap 同一套点分路径)的值改成 $Value,只改那一行。
# 路径按同一套缩进栈规则解析,改的就是 Get-AuthoritativeScalar 会读到的那一行;恰好一处才改,改完再用 Get-YamlScalar 读回核对。
# 任一步不成立就 throw:没改成的负向断言是空转。被改那一行的行尾注释随之丢掉,不影响判定。
function Set-FixtureYamlScalar {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$KeyPath,
        [Parameter(Mandatory = $true)][string]$Value
    )
    $lines = @([System.IO.File]::ReadAllText($Path) -split "`r?`n")
    $stack = New-Object System.Collections.Generic.List[object]
    $hits = New-Object System.Collections.Generic.List[int]
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ([string]::IsNullOrWhiteSpace($lines[$i])) { continue }
        $stripped = Remove-YamlInlineComment -Value ($lines[$i] -replace "`t", '    ')
        if ([string]::IsNullOrWhiteSpace($stripped)) { continue }
        $indent = $stripped.Length - $stripped.TrimStart(' ').Length
        $content = $stripped.Trim()
        if ($content.StartsWith('---') -or $content.StartsWith('...')) { continue }
        # 与解析器同序:序列元素只弹栈不入栈;既不是元素也不是「键:」的行不动栈。
        $isSequenceItem = $content.StartsWith('- ') -or $content -eq '-'
        if (-not $isSequenceItem -and -not [regex]::IsMatch($content, '^[^:]+:')) { continue }
        while ($stack.Count -gt 0 -and $stack[$stack.Count - 1].Indent -ge $indent) { $stack.RemoveAt($stack.Count - 1) }
        if ($isSequenceItem) { continue }
        $key = ConvertFrom-YamlScalarLiteral -Value ([regex]::Match($content, '^(?<k>[^:]+):').Groups['k'].Value)
        $stack.Add(@{ Indent = $indent; Key = $key })
        if ((@($stack | ForEach-Object { $_.Key }) -join '.') -ceq $KeyPath) { $hits.Add($i) }
    }
    if ($hits.Count -ne 1) { throw "夹具:$Path 里键路径 $KeyPath 应恰好 1 处,实际 $($hits.Count) 处" }
    $lines[$hits[0]] = [regex]::Replace($lines[$hits[0]], '^(?<head>[^:]+:).*$', ('${head} ' + $Value))
    [System.IO.File]::WriteAllText($Path, ($lines -join "`n"), [System.Text.UTF8Encoding]::new($false))
    $readBack = Get-YamlScalar -Path $Path -KeyPath $KeyPath
    if (-not $readBack.Found -or $readBack.Value -cne $Value) { throw "夹具:$Path 的 $KeyPath 没有改成 '$Value'(读回 '$($readBack.Value)')" }
}

Test-Case '负向:ConfigMap 要镜像的 MySQL 池参数不合格时,入口门禁 Assert-MysqlPoolMirror throw 并点名文件与键路径(夹具是临时目录里的配置副本,不碰仓库)' {
    # spec_G2b §5:池参数不合格要在任何 apply 之前 throw,不留半截部署。Go 服务的空闲数下限是 1(0 在 Go 侧 = 没配、取默认),
    # Hikari minimum-idle 的下限是 0;ExtraStoreConnMaxIdleSeconds 必须为正(Go 侧把 ≤0 当成没写)。每次只改一个键,其余保持仓库原值。
    # 两个「超过上限」的值按仓库当前上限 + 1 取:调池上限不需要来改本用例。
    $routeStoreMaxOpen = [int](Get-EtcValue -RelativePath 'go/data_service/etc/data_service.yaml' -KeyPath 'RouteStore.MaxOpenConn')
    $hikariMaxPool = [int](Get-EtcValue -RelativePath 'java/gateway_node/src/main/resources/application.yaml' -KeyPath 'spring.datasource.hikari.maximum-pool-size')
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('mysql-pool-mirror-' + [guid]::NewGuid().ToString('N'))
    $resetFixture = {
        foreach ($rel in $mysqlPoolMirrorFixtureFiles) {
            $dst = Join-Path $tempRoot $rel
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
            Copy-Item -LiteralPath (Join-Path (Get-RepoRoot) $rel) -Destination $dst -Force
        }
    }
    try {
        # Get-AuthoritativeScalar 按动态作用域读 $RepoRoot(在 k8s_deploy.ps1 里是脚本级变量)。
        $RepoRoot = $tempRoot
        # 前提:副本不改就该放行。否则下面几条负向断言分不清是谁造成的失败。
        & $resetFixture
        Assert-MysqlPoolMirror

        foreach ($bad in @(
            @{ Why = 'RouteStore 空闲数超过上限'; File = 'go/data_service/etc/data_service.yaml'; KeyPath = 'RouteStore.MaxIdleConn'; Value = [string]($routeStoreMaxOpen + 1) }
            @{ Why = 'PlayerName 上限为 0'; File = 'go/data_service/etc/data_service.yaml'; KeyPath = 'PlayerName.MaxOpenConn'; Value = '0' }
            @{ Why = 'zone 家族额外库空闲数为 0(Go 侧下限 1)'; File = 'go/db/etc/db.yaml'; KeyPath = 'Placement.ZoneStoreMaxIdleConn'; Value = '0' }
            @{ Why = '额外库空闲连接回收秒数为 0'; File = 'go/db/etc/db.yaml'; KeyPath = 'Placement.ExtraStoreConnMaxIdleSeconds'; Value = '0' }
            @{ Why = 'Hikari minimum-idle 为负(下限 0)'; File = 'java/gateway_node/src/main/resources/application.yaml'; KeyPath = 'spring.datasource.hikari.minimum-idle'; Value = '-1' }
            @{ Why = 'Hikari minimum-idle 超过 maximum-pool-size'; File = 'java/gateway_node/src/main/resources/application.yaml'; KeyPath = 'spring.datasource.hikari.minimum-idle'; Value = [string]($hikariMaxPool + 1) }
        )) {
            & $resetFixture
            Set-FixtureYamlScalar -Path (Join-Path $tempRoot $bad.File) -KeyPath $bad.KeyPath -Value $bad.Value
            $message = ''
            try { Assert-MysqlPoolMirror } catch { $message = $_.Exception.Message }
            Assert-Match -Text $message -Pattern 'MySQL 连接池镜像' -Because "$($bad.Why):入口门禁必须 throw(fail-closed),不能把非法池值镜像进 ConfigMap"
            Assert-Match -Text $message -Pattern ([regex]::Escape("$($bad.File) 的 $($bad.KeyPath)='$($bad.Value)'")) -Because "$($bad.Why):错误必须点名文件、键路径与值"
        }

        # 对照:Hikari minimum-idle=0 是合法值(不保留空闲连接),门禁放行并原样镜像 0。与上面 Go 服务的下限 1 一起,两个下限都钉住。
        & $resetFixture
        Set-FixtureYamlScalar -Path (Join-Path $tempRoot 'java/gateway_node/src/main/resources/application.yaml') -KeyPath 'spring.datasource.hikari.minimum-idle' -Value '0'
        Assert-MysqlPoolMirror
        $gatewayMirror = Get-MysqlPoolMirror -Target 'gateway'
        Assert-Equal -Expected 0 -Actual $gatewayMirror['spring.datasource.hikari.minimum-idle'] -Because 'Hikari minimum-idle=0 必须放行,并原样镜像 0'
    }
    finally {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Test-Case '写路径入口:Assert-MysqlPoolMirror 挂在 zone-up / all-up / infra-up 门禁块里,先于 switch ($Command)' {
    # 生成器取值时会再核一次,但那已经在 Apply-Zone 中途(node-config / gate / scene 都 apply 完了)。上面的负向用例只证明门禁函数会 throw,
    # 这里钉住它确实挂在写路径入口、且先于任何集群写操作(写操作都在 switch ($Command) 的分支里)。与 Write-MysqlConnectionLedger 的先后无关。
    $writeGates = @($deadlineDeployAst.EndBlock.Statements | Where-Object {
        $_ -is [System.Management.Automation.Language.IfStatementAst] -and
        $_.Clauses[0].Item1.Extent.Text -match '^\$Command\s+-in\s+@\(\s*"zone-up"\s*,\s*"all-up"\s*,\s*"infra-up"\s*\)$'
    })
    Assert-Equal -Expected 1 -Actual $writeGates.Count -Because 'k8s_deploy.ps1 顶层应当恰好有一个 if ($Command -in @("zone-up", "all-up", "infra-up")) 写路径门禁块'
    $mirrorCalls = @($writeGates[0].Clauses[0].Item2.FindAll({ param($a) $a -is [System.Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'Assert-MysqlPoolMirror' }, $true))
    Assert-Equal -Expected 1 -Actual $mirrorCalls.Count -Because '写路径门禁块里必须调用一次 Assert-MysqlPoolMirror'
    $commandSwitches = @($deadlineDeployAst.EndBlock.Statements | Where-Object {
        $_ -is [System.Management.Automation.Language.SwitchStatementAst] -and $_.Condition.Extent.Text -match '^\(?\s*\$Command\s*\)?$'
    })
    Assert-Equal -Expected 1 -Actual $commandSwitches.Count -Because 'k8s_deploy.ps1 顶层应当恰好有一个 switch ($Command)'
    Assert-True -Condition ($mirrorCalls[0].Extent.StartOffset -lt $commandSwitches[0].Extent.StartOffset) -Because 'Assert-MysqlPoolMirror 必须先于 switch ($Command)(Apply-Infra / Apply-Zone 的集群写操作都在它的分支里)'
}

Test-Case 'mysql-connection-alerts.yaml:PrometheusRule 形状,额外库数阈值与 Get-MysqlConnectionPlan 同源' {
    $alertsRelativePath = 'deploy/k8s/mysql-connection-alerts.yaml'
    $alertsPath = Join-Path (Get-RepoRoot) $alertsRelativePath
    Assert-True -Condition (Test-Path -LiteralPath $alertsPath) -Because "$alertsRelativePath 应当存在(MySQL 连接预算与连接池的告警规则)"
    # 只看非注释行,统一成 LF 后按行锚定:文件头与各条规则上方的注释里也会提到规则名、severity 与阈值,不能让注释把断言喂饱。
    $effective = @((Get-Content -LiteralPath $alertsPath -Raw) -split "`r?`n" | Where-Object { $_ -notmatch '^\s*#' }) -join "`n"
    Assert-Match -Text $effective -Pattern '(?m)^kind:[ ]*PrometheusRule[ ]*$' -Because '必须是 PrometheusRule(与 owner-epoch-alerts.yaml 同一安装方式)'
    Assert-Match -Text $effective -Pattern '(?m)^metadata:[ ]*\n[ ]+name:[ ]*mysql-connection-alerts[ ]*$' -Because 'metadata.name 必须是 mysql-connection-alerts'

    $alerts = [regex]::Matches($effective, '(?m)^[ ]*- alert:[ ]*(?<name>\S+)')
    Assert-Equal -Expected 5 -Actual $alerts.Count -Because ("应当恰好 5 条规则(MySQL 服务端侧 2 条 + 客户端连接池侧 3 条),实际:{0}" -f (@($alerts | ForEach-Object { $_.Groups['name'].Value }) -join ', '))
    for ($i = 0; $i -lt $alerts.Count; $i++) {
        $bodyEnd = if ($i + 1 -lt $alerts.Count) { $alerts[$i + 1].Index } else { $effective.Length }
        $body = $effective.Substring($alerts[$i].Index, $bodyEnd - $alerts[$i].Index)
        Assert-Match -Text $body -Pattern '(?m)^[ ]+severity:[ ]*\S+' -Because "规则 $($alerts[$i].Groups['name'].Value) 必须带 labels.severity(严重级约定见文件头)"
    }

    # DbExtraStorePoolsBeyondPlan 的两个阈值就是规划档的额外库数:规划档改了而阈值没跟(或反过来),这条告警要么恒响、要么该响不响。
    # 提取依赖告警文件「每个阈值单独占一行,形如 pool="extra_zone"}) > 3」的写法(文件里该规则上方的注释写明了这条约定)。
    # 把表达式改写成别的形状会让本条变红 —— 有意为之:要么保持这个形状,要么同步改这里的提取方式。
    $plan = Get-MysqlConnectionPlan
    foreach ($threshold in @(
        @{ Pool = 'extra_zone'; PlanKey = 'ExtraZoneStores' }
        @{ Pool = 'extra_non_zone'; PlanKey = 'NonZoneStores' }
    )) {
        $thresholdPattern = 'pool="' + [regex]::Escape($threshold.Pool) + '"\}\) > (?<n>\d+)'
        $found = [regex]::Matches($effective, $thresholdPattern)
        Assert-Equal -Expected 1 -Actual $found.Count -Because "$alertsRelativePath 里 pool=`"$($threshold.Pool)`" 的阈值应当恰好出现 1 处(形如 pool=`"$($threshold.Pool)`"}) > N,单独占一行),实际 $($found.Count) 处"
        Assert-Equal -Expected $plan[$threshold.PlanKey] -Actual $found[0].Groups['n'].Value -Because "pool=`"$($threshold.Pool)`" 的阈值必须等于 Get-MysqlConnectionPlan 的 $($threshold.PlanKey):改规划档必须同改告警阈值(反之亦然),并重算 max_connections"
    }
}

Test-Case '普通 gate/scene Deployment 必须先创建被 emptyDir 遮蔽的日志父目录' {
    foreach ($name in @('gate','scene')) {
        $block = Select-ManifestByName -Output $devOut -Name $name
        Assert-Match -Text $block -Pattern ('args: \["mkdir -p /app/bin/logs/cpp_nodes && .*&& \./' + $name + '"\]') -Because 'Node 构造器在初始化前就打开 logs/cpp_nodes/<role>，父目录必须已经存在'
    }
}
Test-Case 'Agones Scene Fleet 同样必须在进程启动前建立日志父目录' {
    $fleet = Select-ManifestByName -Output $agonesOut -Name 'scene'
    Assert-Match -Text $fleet -Pattern 'kind: Fleet' -Because '必须检查真正的 Fleet 而非普通 Deployment'
    Assert-Match -Text $fleet -Pattern 'args: \["mkdir -p /app/bin/logs/cpp_nodes && .*&& \./scene"\]' -Because 'Fleet 的 emptyDir 与 Deployment 同样遮蔽镜像目录'
}

# ─────────────────────────────────────────────────────────────────
# C++ 日志 sidecar(见 docs/ops/grafana-loki-local-logs.md §6)
# ─────────────────────────────────────────────────────────────────

Test-Case '日志保留:业务容器必须自带清理循环,且 node-logs 有 sizeLimit 兜底' {
    foreach ($name in @('gate','scene')) {
        $block = Select-ManifestByName -Output $devOut -Name $name
        # muduo 每 8MiB 滚一个新文件且从不删旧文件;没有这两道闸,长跑节点会把节点盘写满,
        # 而本机 kind 的 evictionHard 全是 0%,盘满的表现是 MySQL/Kafka/etcd 一起 ENOSPC。
        Assert-Match -Text $block -Pattern 'ls -1t /app/bin/logs/cpp_nodes/\*\.log .*tail -n \+9 .*rm -f' -Because '业务容器里必须有"只留最近若干个日志文件"的清理循环'
        Assert-Match -Text $block -Pattern 'name: node-logs\s+emptyDir: \{ sizeLimit: \S+ \}' -Because 'emptyDir 不封顶 = 一个 Pod 能把整个节点盘写满'
    }
}

Test-Case 'sidecar 必须是 initContainers 里的原生 sidecar(restartPolicy: Always)' {
    # 普通容器形态下:Agones 给 GameServer Pod 写死 restartPolicy: Never,sidecar OOM 后永不重启
    # 且 Agones 只看游戏容器不会报错;Pod 终止时它又与业务容器同时收 SIGTERM,drain 期日志整段丢。
    foreach ($pair in @(@{ Out = $devOut; Name = 'gate' }, @{ Out = $agonesOut; Name = 'scene' })) {
        $block = Select-ManifestByName -Output $pair.Out -Name $pair.Name
        Assert-Match -Text $block -Pattern '(?s)initContainers:\s+- name: log-sidecar.*?restartPolicy: Always' -Because 'sidecar 必须放 initContainers 并显式声明 restartPolicy: Always'
        # 只能有一份:普通容器带 restartPolicy 要 API server 开了 ContainerRestartRules 门控才收,
        # 1.28~1.33 会报 containers[N].restartPolicy: Forbidden,整份清单 apply 不上去。
        Assert-Equal -Expected 1 -Actual ([regex]::Matches($block, '- name: log-sidecar').Count) -Because 'sidecar 容器片段只应出现在 initContainers 一处'
        Assert-Match -Text $block -Pattern 'runAsNonRoot: true' -Because '观测容器没有理由把 root 塞回业务 Pod'
        # 值带引号(全数字哈希不加引号会被 YAML 解析成数字、API server 拒收),两种写法都认。
        Assert-Match -Text $block -Pattern 'mmorpg\.io/cpp-log-sidecar-config-hash: "?[0-9a-f]{12}"?' -Because '配置只改 ConfigMap 不会让 Alloy 重读,必须靠 pod 模板注解触发滚动'
    }
}

Test-Case 'k8s label value 契约:生成的标签值不能是裸 - (YAML 非法 + label 校验器拒)' {
    # 真实事故:battle 路径曾用 "-" 当 zone 占位,渲染出 `mmorpg.io/zone: -`,
    # YAML 把行尾裸 - 当块序列项,infra-up / all-up 在这里整体中断。
    # 注意 kubectl apply --dry-run=client 查不出 label value 非法(它只按 schema 校验),
    # 所以这条必须在生成期断。
    foreach ($out in @($devOut, $agonesOut, $battleInfraRun.Output)) {
        Assert-NotMatch -Text $out -Pattern '(?m)^\s+[A-Za-z0-9][-A-Za-z0-9_./]*:\s+-\s*$' -Because '标签/字段值为裸 - 既是非法 YAML 也是非法 label value'
    }
}

Test-Case 'battle 是全局池:它的 sidecar ConfigMap 不能带 zone 标签,zone 的必须带且加引号' {
    $zoneCm = Select-ManifestByName -Output $devOut -Name 'cpp-log-sidecar'
    Assert-Match -Text $zoneCm -Pattern 'mmorpg\.io/zone: "contract-test"' -Because 'zone 标签值要加引号,否则将来纯数字 zone 名会被 kubectl 当数字拒掉'
    $battleCm = Select-ManifestByName -Output $battleInfraRun.Output -Name 'cpp-log-sidecar'
    Assert-NotMatch -Text $battleCm -Pattern 'mmorpg\.io/zone' -Because 'battle 不属于任何 zone;写个占位值会被 {mmorpg.io/zone=<真 zone>} 的选择器误选'
}

Test-Case '-NoCppLogSidecar 必须产出与加这个功能之前一致的清单' {
    $run = Invoke-DeployDryRun -Arguments ($BaseArgs + @('-NoCppLogSidecar','-SkipGoSvc','-SkipJavaSvc'))
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because '关掉 sidecar 的路径必须照样能生成'
    Assert-NotMatch -Text $run.Output -Pattern 'log-sidecar' -Because '关掉后不能残留任何 sidecar 容器/卷'
    Assert-NotMatch -Text $run.Output -Pattern 'initContainers' -Because '关掉后不能留下空的 initContainers 键(空列表 = null,API server 拒)'
    Assert-NotMatch -Text $run.Output -Pattern 'cpp-log-sidecar-config-hash' -Because '关掉后 pod 模板不该带 sidecar 配置注解'
}

Test-Case 'Java gateway 冷启动必须有独立 startupProbe 预算并保留就绪与存活探针' {
    $block = Select-ManifestByName -Output $devOut -Name 'gateway'
    Assert-Match -Text $block -Pattern 'startupProbe:\s+httpGet:\s+path: /actuator/health/readiness\s+port: 8081\s+periodSeconds: 10\s+timeoutSeconds: 5\s+failureThreshold: 60' -Because 'Spring/JPA 初始化期间不能被 liveness 的短失败预算反复杀死'
    Assert-Match -Text $block -Pattern 'readinessProbe:\s+httpGet:\s+path: /actuator/health/readiness\s+port: 8081\s+initialDelaySeconds: 15\s+periodSeconds: 10' -Because '冷启动后仍必须保留 HTTP readiness 门禁'
    Assert-Match -Text $block -Pattern 'livenessProbe:\s+httpGet:\s+path: /actuator/health/liveness\s+port: 8081\s+initialDelaySeconds: 30\s+periodSeconds: 20' -Because 'startupProbe 不能取代运行期存活检查'
    # liveness 不能落到聚合的 /actuator/health:那会把 DB / Redis 等外部依赖的抖动算进存活,依赖一抖就重启全部副本。
    Assert-NotMatch -Text $block -Pattern 'path: /actuator/health\s' -Because '探针必须走 readiness / liveness 分组,不能用聚合健康端点'
}
Test-Case 'Java gateway 必须显式限制 JVM 堆并为 native 内存保留容器预算' {
    $block = Select-ManifestByName -Output $devOut -Name 'gateway'
    Assert-Match -Text $block -Pattern 'name: JAVA_TOOL_OPTIONS\s+value: "-Xms64m -Xmx384m"' -Because '不能依赖本机 JRE 对 cgroup 内存限制的自动识别'
    Assert-Match -Text $block -Pattern 'requests:\s+cpu: 200m\s+memory: 512Mi' -Because '调度请求必须覆盖已观测的启动期常驻内存'
    Assert-Match -Text $block -Pattern 'limits:\s+cpu: "1"\s+memory: 1Gi' -Because '堆外内存包括 metaspace、线程栈与直接缓冲区，不能把最大堆等同容器内存'
}

# ─────────────────────────────────────────────────────────────────
# gateway 管理面口令(admin.api-key)只经 Secret 进容器(k8s-client-entry D91 纵深防御)
# ─────────────────────────────────────────────────────────────────
# 链路:环境变量 MMORPG_GATEWAY_ADMIN_API_KEY → zone namespace 的 Secret gateway-admin-api-key(key api-key)→ gateway 容器
# env ADMIN_APIKEY(secretKeyRef)→ 挂载的 application.yaml 里 admin.api-key: "${ADMIN_APIKEY}"。实现在 k8s_deploy.ps1 的
# Initialize-InjectedSecrets / New-JavaSvcConfigMapYaml / Apply-JavaSvcManifests / Apply-InjectedSecret / Add-PodTemplateAnnotation /
# Get-InjectedSecretFingerprint;这里只断 DryRun 的可观察输出。非 DryRun(假 kubectl)下缺失 / 占位 / 过短的整句报错与"零写操作"
# 由 k8s_client_entry_contract.tests.ps1 钉住,本节补 DryRun 路径与产物形状。
# 两次 dev 运行都显式给出该变量(空串 = 未设置,回落占位值),结论不受跑测机器上残留的同名环境变量影响;
# 都带 -SkipGoSvc:本节只看 Java 服务的产物。
$AdminKeyDevArgs = $BaseArgs + @('-SkipGoSvc')
$adminDevPlaceholderRun = Invoke-DeployDryRun -Arguments $AdminKeyDevArgs -Env @{ MMORPG_GATEWAY_ADMIN_API_KEY = '' }
$adminDevSameKeyRun = Invoke-DeployDryRun -Arguments $AdminKeyDevArgs -Env @{ MMORPG_GATEWAY_ADMIN_API_KEY = $ProdEnv.MMORPG_GATEWAY_ADMIN_API_KEY }

# 取 gateway Deployment Pod 模板上的口令指纹注解值。形状不对(缺失 / 多于一处 / 不在 template.metadata.annotations 下 /
# 没加双引号 / 不是 12 位小写十六进制)一律 throw,不返回半截结果。
function Get-GatewayAdminApiKeyHash {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Output,
        [Parameter(Mandatory = $true)][string]$Label
    )
    $block = Select-ManifestByName -Output $Output -Name 'gateway'
    if ($null -eq $block) { throw "${Label}:DryRun 输出里没有 gateway 的 manifest" }
    $count = [regex]::Matches($block, 'mmorpg\.io/gateway-admin-api-key-hash').Count
    if ($count -ne 1) { throw "${Label}:gateway manifest 里口令指纹注解应恰好 1 处,实际 $count 处" }
    # Add-PodTemplateAnnotation 紧跟 template: 下的 metadata: 插入(前面可有一行生成说明注释)。值必须带双引号:
    # 12 位十六进制可能全是数字或形如 1e5…,不加引号会被 YAML 当成数字,而注解值只能是字符串,kubectl 拒收。
    # Get-ManifestBlocks 已把块内换行统一成 LF,所以这里按 \n 逐行锚定。
    $m = [regex]::Match($block, '(?m)^[ ]+template:[ ]*\n[ ]+metadata:[ ]*\n(?:[ ]*#[^\n]*\n)*[ ]+annotations:[ ]*\n[ ]+mmorpg\.io/gateway-admin-api-key-hash: "(?<hash>[0-9a-f]{12})"[ ]*$')
    if (-not $m.Success) { throw "${Label}:口令指纹注解必须在 Pod 模板 metadata.annotations 下,值为加双引号的 12 位小写十六进制" }
    return $m.Groups['hash'].Value
}

Test-Case '负向:prod 档位部署 gateway 却缺 MMORPG_GATEWAY_ADMIN_API_KEY 必须在任何写操作之前拒绝并点名该变量' {
    $env2 = @{} + $ProdEnv
    $env2['MMORPG_GATEWAY_ADMIN_API_KEY'] = ''
    $run = Invoke-DeployDryRun -Arguments $ProdArgs -Env $env2

    Assert-True -Condition ($run.ExitCode -ne 0) -Because "缺管理面口令必须阻断:否则 /admin/** 用 git 里公开的 change-me-in-production 鉴权。输出: $($run.Output)"
    # 其余生产密钥都已给齐($ProdEnv 基线能出产物),拒绝只能来自这一把;断 Resolve-InjectedSecret"未设置"分支的原文开头。
    Assert-Match -Text $run.Output -Pattern 'ReleaseProfile=prod 要求从环境变量 MMORPG_GATEWAY_ADMIN_API_KEY' -Because '错误必须点名缺哪个环境变量,且说清是注入缺失而不是别的失败'
    Assert-NotMatch -Text $run.Output -Pattern '\[dry-run\] kubectl [^\r\n]*\b(apply|create|delete|patch|replace)\b' -Because '口令解析(Initialize-InjectedSecrets)先于任何写操作,不留半截部署'
    Assert-NotMatch -Text $run.Output -Pattern '# Secret gateway-admin-api-key' -Because '拒绝之后不得再写管理面口令 Secret'
}

Test-Case 'dev 档未设 MMORPG_GATEWAY_ADMIN_API_KEY:回落占位值并打警告,Secret gateway-admin-api-key 只打隐值的 dry-run 行,gateway 经 secretKeyRef 注入 ADMIN_APIKEY' {
    $run = $adminDevPlaceholderRun
    Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dev 档允许回落占位值,不能拒绝(本地栈要能一键起)。输出: $($run.Output)"
    # 只断警告里不含空格的一段:Write-Warning 若被 ConsoleHost 按窗口宽度折行(重定向下是否折行未实测),折点在空格处;这一段连同前缀
    # "WARNING: gateway " 止于第 64 显示列(中文按 2 列),≥ 约 65 列的控制台不会把它拆开。旧写法连到"未设置或仍是占位串",
    # 而其后直到 "/admin/**" 是一整段无空格 token(止于约第 113 列),窗口不足约 114 列就会在"未设置"前折断。
    Assert-Match -Text $run.Output -Pattern 'admin password is a placeholder \(MMORPG_GATEWAY_ADMIN_API_KEY' -Because '回落占位值必须明说:此时 /admin/** 用公开常量鉴权,只允许本地 dev'
    # DryRun 下 Apply-InjectedSecret 只打一行隐去值的意图,不走会把整份 YAML(含明文 stringData)打出来的 Invoke-KubectlWithInputFile。
    Assert-Match -Text $run.Output -Pattern '(?m)^\[dry-run\] kubectl apply -n \S+ -f -\s+# Secret gateway-admin-api-key \(key api-key; value hidden\)\s*$' -Because 'Secret 的 dry-run 行要点名 Secret 名与 key,且声明值已隐去'

    $gateway = Select-ManifestByName -Output $run.Output -Name 'gateway'
    Assert-True -Condition ($null -ne $gateway) -Because 'DryRun 输出里应当有 gateway 的 manifest'
    # 不写 optional(= false):Secret 缺失时 Pod 卡在 CreateContainerConfigError,而不是带着空口令起来。
    Assert-Match -Text $gateway -Pattern '- name: ADMIN_APIKEY\s+valueFrom:\s+secretKeyRef:\s+name: gateway-admin-api-key\s+key: api-key[ ]*\n(?![ ]+optional:)' -Because 'ADMIN_APIKEY 必须经 secretKeyRef 取 Secret gateway-admin-api-key 的 api-key,且不得 optional'
    Assert-Equal -Expected 1 -Actual ([regex]::Matches($gateway, 'name: ADMIN_APIKEY').Count) -Because 'ADMIN_APIKEY 只能注入一份,重复的 env 键以哪条为准不该交给 kubelet 决定'
    Assert-NotMatch -Text $gateway -Pattern 'name: ADMIN_APIKEY\s+value:' -Because '口令不得以明文 value 写进 Deployment'
}

Test-Case 'gateway ConfigMap 的 admin.api-key 只写 ${ADMIN_APIKEY} 引用(dev / prod 同形),DryRun 输出里没有口令明文' {
    # 显式写引用而不靠 Spring 宽松绑定,设计意图是:env 注入一旦被删 / 改名,占位符解析不了 gateway 直接拒启(fail-closed),
    # 而不是静默回落到 jar 内公开的 change-me-in-production。
    # (Spring 运行期行为未验证:外部 ./config 覆盖 jar 内配置、嵌套占位符解析不了即拒启,都只按 Spring Boot 文档核对过,
    # 见 ingress2d 批 fix:deploy-hardening 的 RISKS;待部署后对 /admin 实测。本用例只断 ConfigMap 文本。)
    foreach ($case in @(@{ Label = 'dev'; Output = $devOut }, @{ Label = 'prod'; Output = $prodRun.Output })) {
        $flat = ConvertTo-FlatManifest -Block (Select-ManifestByName -Output $case.Output -Name 'java-svc-gateway-config')
        # 用 -ceq 而不是 Assert-Equal:后者按 -ne 比较、不区分大小写,生成器模板漂成 ${admin_apikey} 也会放过;
        # 这里钉的是生成器写出的字面量,与 manifest 注入的 env 名 ADMIN_APIKEY 逐字一致。
        $apiKeyRef = Get-FlatValue -Flat $flat -KeyPath 'data.application.yaml.admin.api-key'
        Assert-True -Condition ($apiKeyRef -ceq '${ADMIN_APIKEY}') -Because "$($case.Label) 档 java-svc-gateway-config 的 admin.api-key 必须逐字(区分大小写)是对容器 env ADMIN_APIKEY 的引用,不得写口令值(实际 '$apiKeyRef')"
    }
    $plain = [regex]::Escape($ProdEnv.MMORPG_GATEWAY_ADMIN_API_KEY)
    foreach ($case in @(@{ Label = 'prod 基线'; Output = $prodRun.Output }, @{ Label = 'dev 注入同一口令'; Output = $adminDevSameKeyRun.Output })) {
        Assert-NotMatch -Text $case.Output -Pattern $plain -Because "$($case.Label):口令只经 Secret 进容器,ConfigMap / Secret 意图行 / 指纹注解都不得带明文"
    }
}

Test-Case 'gateway Pod 模板带口令指纹注解(加引号的 12 位十六进制):同一口令两次独立运行指纹相同,换口令指纹必变(轮换自动滚动 gateway)' {
    foreach ($run in @($adminDevPlaceholderRun, $adminDevSameKeyRun)) {
        Assert-Equal -Expected 0 -Actual $run.ExitCode -Because "dev 档 DryRun 应当成功。输出: $($run.Output)"
    }
    $placeholderHash = Get-GatewayAdminApiKeyHash -Output $adminDevPlaceholderRun.Output -Label 'dev 占位口令'
    $devHash = Get-GatewayAdminApiKeyHash -Output $adminDevSameKeyRun.Output -Label 'dev 注入与 prod 基线相同的口令'
    $prodHash = Get-GatewayAdminApiKeyHash -Output $prodRun.Output -Label 'prod 基线'
    # 指纹只由口令(加 Secret 名做域分隔)决定:两次独立子进程、不同档位,同一口令必须得到同一指纹 ——
    # 否则每次重跑 zone-up 都无故滚动 gateway。
    Assert-Equal -Expected $prodHash -Actual $devHash -Because '同一口令的指纹必须稳定'
    # secretKeyRef 只在容器启动时读:只改 Secret 时 Pod 模板逐字节不变,apply 是 no-op,旧口令一直有效、新口令被拒。
    Assert-True -Condition ($placeholderHash -ne $devHash) -Because "换口令指纹必须变,否则轮换不会触发滚动(两者都是 $devHash)"
    Assert-NotMatch -Text $adminDevSameKeyRun.Output -Pattern 'admin password is a placeholder' -Because '对照:非占位口令不该打占位警告'
}

Test-Case 'gateway 写入顺序 ConfigMap → Secret → Deployment,Secret 与 Deployment 同 namespace(secretKeyRef 非 optional,Secret 必须先落地)' {
    $out = $adminDevPlaceholderRun.Output
    $cm = [regex]::Match($out, '(?m)^  name: java-svc-gateway-config\s*$')
    $secrets = [regex]::Matches($out, '(?m)^\[dry-run\] kubectl apply -n (?<ns>\S+) -f -\s+# Secret gateway-admin-api-key \(')
    $deployEnv = [regex]::Match($out, '(?m)^[ ]+- name: ADMIN_APIKEY\s*$')
    Assert-True -Condition $cm.Success -Because 'DryRun 输出里应当有 java-svc-gateway-config 的 metadata.name'
    Assert-Equal -Expected 1 -Actual $secrets.Count -Because '单 zone 的 zone-up 只写一次管理面口令 Secret'
    Assert-True -Condition $deployEnv.Success -Because 'DryRun 输出里应当有注入 ADMIN_APIKEY 的 gateway Deployment'
    Assert-True -Condition ($cm.Index -lt $secrets[0].Index) -Because 'ConfigMap 先于 Secret(Apply-JavaSvcManifests 注释约定的写序 ConfigMap → Secret → Deployment;Apply-OneGoSvc 的 login 口令 Secret 反而先于 ConfigMap,两者只共享"Secret 先于 Deployment")'
    Assert-True -Condition ($secrets[0].Index -lt $deployEnv.Index) -Because 'Secret 必须先于引用它的 Deployment:反过来新 Pod 会卡在 CreateContainerConfigError'

    # Deployment 所在的 apply 行 = 它之前最近的一条带 manifest 的 dry-run apply 行(Secret 意图行以注释结尾,不会被这条模式选中)。
    $deployApply = @([regex]::Matches($out, '(?m)^\[dry-run\] kubectl [^\r\n]*apply -n (?<ns>\S+) -f -\s*$') | Where-Object { $_.Index -lt $deployEnv.Index }) | Select-Object -Last 1
    Assert-True -Condition ($null -ne $deployApply) -Because 'gateway Deployment 之前应当有它自己的 dry-run apply 行'
    Assert-Equal -Expected $deployApply.Groups['ns'].Value -Actual $secrets[0].Groups['ns'].Value -Because 'secretKeyRef 只能引用同 namespace 的 Secret'
}

Test-Case 'Java 服务缺 manifest(目录里的 auth)时连 ConfigMap 也不 apply:DryRun 不含 java-svc-auth-config' {
    $authManifest = Join-Path (Get-RepoRoot) 'deploy/k8s/manifests/java-svc/auth.yaml'
    Assert-True -Condition (-not (Test-Path -LiteralPath $authManifest)) -Because '本条前提:auth 在 Java 服务目录里但没有 manifest。有人补上 auth.yaml 后,本条应改为断言它的 ConfigMap 与 Deployment 成对出现'
    Assert-True -Condition ($null -ne (Select-ManifestByName -Output $devOut -Name 'java-svc-gateway-config')) -Because '对照:Java 服务路径确实走到了(gateway 的 ConfigMap 在),auth 的缺席才有意义'
    foreach ($case in @(@{ Label = 'dev'; Output = $devOut }, @{ Label = 'prod'; Output = $prodRun.Output })) {
        Assert-NotMatch -Text $case.Output -Pattern 'java-svc-auth-config' -Because "$($case.Label):没有 Deployment 消费的孤儿 ConfigMap 不该 apply(与 Apply-OneGoSvc 同口径)"
    }
}
# ─────────────────────────────────────────────────────────────────
# 路由服预算链(docs/design/client-request-ordering.md §5.2 不变式 I1)
# ─────────────────────────────────────────────────────────────────
# 路由目标 Go 服务的 zrpc Timeout + 1000 ≤ 路由服 ForwardTimeoutMs。判定只有一份:k8s_deploy.ps1 的
# Get-RouterForwardBudgetViolations(纯函数)+ Assert-RouterForwardBudget(读文件、写路径入口调用)。
# 照上面 deadline 预算的做法按 AST 抽出来直接调用,不另抄一份判定,也不执行脚本入口。
$routerBudgetFunctionNames = @('Get-RouterForwardBudgetViolations', 'Assert-RouterForwardBudget')
foreach ($routerBudgetFnAst in $deadlineDeployAst.FindAll({ param($a) $a -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    if ($routerBudgetFunctionNames -contains $routerBudgetFnAst.Name) {
        Set-Item -Path "Function:script:$($routerBudgetFnAst.Name)" -Value $routerBudgetFnAst.Body.GetScriptBlock()
    }
}
foreach ($routerBudgetFnName in $routerBudgetFunctionNames) {
    if (-not (Test-Path "Function:$routerBudgetFnName")) { throw "k8s_deploy.ps1 缺少函数 $routerBudgetFnName(路由服预算守卫被删了?后续断言无意义)" }
}

Test-Case "路由服预算链:仓库配置满足 目标 Go 服务 zrpc Timeout + 1000 ≤ 路由服 ForwardTimeoutMs(部署门禁对当前配置放行)" {
    # 不满足时 Assert 抛出的文本就是失败原因,逐项点名是哪个目标、差多少。
    $RepoRoot = Get-RepoRoot
    Assert-RouterForwardBudget | Out-Null
}

Test-Case "zone-up 写路径入口先过路由服预算门禁,之后才有任何 kubectl 写操作" {
    $okAt = $devOut.IndexOf('Router forward budget OK')
    $firstKubectlAt = $devOut.IndexOf('[dry-run] kubectl')
    Assert-True -Condition ($okAt -ge 0) -Because 'zone-up DryRun 必须经过 Assert-RouterForwardBudget(写路径入口门禁),否则守卫等于没挂'
    Assert-True -Condition ($firstKubectlAt -lt 0 -or $okAt -lt $firstKubectlAt) -Because '预算核对必须先于任何 kubectl 写操作,不留半截部署'
}

Test-Case "自检:路由服预算判定对 修复前 login 的 10000 / 差 1ms / Timeout 0 / 非数字 / MethodTimeouts / ForwardTimeoutMs 缺席或 0 报违例,对等号边界与未写 Timeout 通过(守卫不得静默放行)" {
    $check = {
        param([string]$ForwardRaw, [string]$ServiceText)
        $targets = [ordered]@{
            LoginNodeService = @{ Source = 'fixture/login.yaml'; Scalars = (ConvertFrom-YamlToFlatMap -Text $ServiceText).Scalars }
        }
        return @(Get-RouterForwardBudgetViolations -ForwardTimeoutMsRaw $ForwardRaw -Targets $targets)
    }

    $passes = @(
        @{ Why = 'ForwardTimeoutMs 5000 + Timeout 4000(等号边界)'; Forward = '5000'; Service = "Timeout: 4000" }
        @{ Why = 'ForwardTimeoutMs 5000 + 未写 Timeout(go-zero 默认 2000)'; Forward = '5000'; Service = "Name: login.rpc" }
    )
    foreach ($c in $passes) {
        $got = @(& $check $c.Forward $c.Service)
        Assert-True -Condition ($got.Count -eq 0) -Because ("{0} 应通过,实际报:{1}" -f $c.Why, ($got -join ';'))
    }

    $targetViolations = @(
        @{ Why = '修复前的 login 值 10000(路由服 5000 先回超时,handler 继续提交)'; Forward = '5000'; Service = "Timeout: 10000" }
        @{ Why = '修复前的 match 值 5000(与 ForwardTimeoutMs 相等,没有余量)'; Forward = '5000'; Service = "Timeout: 5000" }
        @{ Why = '差 1ms(余量是 1000)'; Forward = '5000'; Service = "Timeout: 4001" }
        @{ Why = '服务端 Timeout 0(go-zero 不装超时拦截器)'; Forward = '5000'; Service = "Timeout: 0" }
        @{ Why = '服务端 Timeout 非数字'; Forward = '5000'; Service = "Timeout: 4s" }
        @{ Why = '出现 MethodTimeouts(不逐方法核对,宁拒不放)'; Forward = '5000'; Service = "Timeout: 3000`nMethodTimeouts:`n  - FullMethod: /loginpb.ClientPlayerLogin/CreatePlayer`n    Timeout: 9s" }
    )
    foreach ($c in $targetViolations) {
        $got = @(& $check $c.Forward $c.Service)
        Assert-True -Condition ($got.Count -gt 0) -Because "$($c.Why) 必须报违例"
        Assert-Match -Text $got[0] -Pattern '^LoginNodeService:' -Because "$($c.Why) 的违例必须点名是哪个目标"
    }

    $forwardViolations = @(
        @{ Why = 'ForwardTimeoutMs 缺席'; Forward = '' }
        @{ Why = 'ForwardTimeoutMs 0'; Forward = '0' }
        @{ Why = 'ForwardTimeoutMs 非数字'; Forward = '5s' }
    )
    foreach ($c in $forwardViolations) {
        $got = @(& $check $c.Forward "Timeout: 1000")
        Assert-True -Condition ($got.Count -gt 0) -Because "$($c.Why) 必须报违例"
        Assert-Match -Text $got[0] -Pattern 'ForwardTimeoutMs' -Because "$($c.Why) 的违例必须点名 ForwardTimeoutMs"
    }
}

$friendSqlAst = $deadlineDeployAst.Find({ param($a)
    $a -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -eq 'Get-FriendSqlSettings'
}, $false)
if ($null -eq $friendSqlAst) { throw '好友权威 SQL 配置门禁缺失' }
Set-Item -Path Function:script:Get-FriendSqlSettings -Value $friendSqlAst.Body.GetScriptBlock()

Test-Case '全服好友开发可用 MySQL，显式 TiDB 入口始终启用后端验证' {
    $local = Get-FriendSqlSettings -Profile 'dev' -TiDBHost '' -DevelopmentHost 'mysql.test:3306'
    Assert-Equal -Expected 'mysql.test:3306' -Actual $local.Host -Because '开发默认沿用独立测试 MySQL'
    Assert-Equal -Expected 'false' -Actual $local.RequireTiDB -Because '开发不得误称已使用 TiDB'
    foreach ($profile in @('dev', 'staging', 'prod')) {
        $settings = Get-FriendSqlSettings -Profile $profile -TiDBHost 'tidb.global.test:4000' -DevelopmentHost 'mysql.test:3306'
        Assert-Equal -Expected 'tidb.global.test:4000' -Actual $settings.Host -Because '所有副本的显式权威入口必须原样保留'
        Assert-Equal -Expected 'true' -Actual $settings.RequireTiDB -Because '指定 TiDB 必须同时启用实际后端身份验证'
    }
}

Test-Case '全服好友生产缺 TiDB 入口或入口非法时拒绝生成' {
    foreach ($profile in @('staging', 'prod')) {
        $rejected = $false
        try { $null = Get-FriendSqlSettings -Profile $profile -TiDBHost '' -DevelopmentHost 'mysql.test:3306' }
        catch { $rejected = $_.Exception.Message -match 'FriendTiDBHost' }
        Assert-True -Condition $rejected -Because '生产不得悄悄落回 MySQL'
    }
    foreach ($hostValue in @('tidb', 'tidb:0', 'tidb:65536', 'user:pass@tidb:4000', 'tidb:4000/db', "tidb:4000`nMode: dev")) {
        $rejected = $false
        try { $null = Get-FriendSqlSettings -Profile 'prod' -TiDBHost $hostValue -DevelopmentHost 'mysql.test:3306' }
        catch { $rejected = $_.Exception.Message -match 'FriendTiDBHost' }
        Assert-True -Condition $rejected -Because '不能把非法地址、凭据或额外配置写入 ConfigMap'
    }
}

exit (Complete-TestRun -SuiteName "k8s_deploy contract")
