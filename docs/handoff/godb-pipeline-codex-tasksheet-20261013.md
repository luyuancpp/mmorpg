# Codex 任务单：存盘管线第 0 批与 proto2mysql 条件写原语的验证（2026-10-13）

> **版本**：第 2.1 版（2026-10-15）。第 1 版经一轮可执行性评审（9 条意见，核实后全部成立或部分成立），第 2 版逐条改过；第 2.1 版只改一处：负责人 2026-10-15 同意联网下载 Go 模块（「同意 2」），§1 的 `GOPROXY` 随之改为联网，其余命令不变。
> **谁执行**：Codex。Claude 不编译、不测试（AGENTS.md §10.1）。
> **验证什么**：[交接](godb-pipeline-handoff-20261013.md) §3.1（主仓库第 0 批）与 §3.2（proto2mysql `SaveConditional`）。两者都从未编译、未运行。
> **结果交给谁**：把 §6 的回报表填好交给负责人，由负责人贴回 Claude 的对话。Claude 按交接 §3.0 写回文档。
> **本单相对交接 §3 的订正**：
> - 验证对象是提交 `0aa1027bc`（第 0 批的代码与 `d5cbb8a8e` 逐字节相同）。主仓库 HEAD 之后又前进了很多提交（别的会话的改动，其中有两个脚本测试被改过），**不影响本单**：所有命令都在按提交号建的临时 worktree 里跑。
> - 交接里的六条命令不能照抄顺序执行（每行都以相对路径 `cd` 开头）。本单每条命令各自写明工作目录。
> - 主工作区有别的会话的未提交改动，不在主工作区跑。
> - 两个 Go 包此前没有任何运行记录，「无新增失败」必须靠基线对比得出；另加一组「基线 + 只拷本批文件」的隔离运行，避免同一提交里别的会话的代码挡住本批的结论。
> - `test_stress_summarize.ps1` 没有 `#requires -Version 7`，它在内部按名字再起一个 `pwsh`，所以 `pwsh` 必须在 PATH 上。

---

## 0. 规则

- 只执行本单列出的命令。任何一步失败：不再执行后续验证步骤；先按 §5 存好输出，需要时做那一次补跑取证；然后照常执行 §2.6 / §3.5 的收尾，再停下报告。**不连续重试、不改断言、不跳过用例、不改代码。**
- 不 commit、不 push、不打 tag；不改任何 `go.mod` / `go.sum`，不运行 `go mod tidy`、`go get`、`go env -w`。
- 不碰主工作区 `D:\luyuan\xuanming-server-mmo-cpp` 里的文件（那里有别的会话在写）。不复用、不清理已有的 worktree `C:\Users\luyua\.codex\worktrees\f452\…`（它停在 `73b852f32` 且带别人的未提交改动）。本单只清理自己建的三个临时 worktree 和两个测试容器。
- Shell 用 PowerShell 7（`pwsh`）。不要用 Windows PowerShell 5.1 或 cmd。
- **所有代码块在同一个 pwsh 进程里执行。** 做不到时，在每个代码块第一行重设 §1 的环境变量（以及用到的 PATH、`PROTO2MYSQL_*`）。
- **本单需要回环网络**：go/db 的既有用例用 miniredis 监听 127.0.0.1 的随机端口；任务 B 连 127.0.0.1:13307 / 14000。沙箱默认拦回环时先向负责人申请放行。
- 被 Windows Application Control 拦截测试 exe 时（事件 3077 / 3033），记录事件号与被拦路径后停下，不绕过。

### 0.1 负责人的同意（截至 2026-10-15）

| # | 事项 | 状态 |
|---|---|---|
| 同意 1 | 安装 Go | **负责人已自行安装 Go 1.27.2**（`C:\Program Files\Go\bin`，可能不在 PATH 上，见 §1）|
| 同意 2 | 联网下载 Go 模块 | **已同意**（负责人 2026-10-15 在 Claude 对话里答复「Codex 跑测试时下载」）。§1 的 `GOPROXY` 设为 `https://goproxy.cn,direct`；范围只限 `go test` / `go vet` 按 go.sum 自动拉取已登记的依赖，§0 第二条（不改 go.mod / go.sum、不 tidy、不 get、不 `go env -w`）不变 |
| 同意 3 | 拉取 `mysql:8.4` 与 `pingcap/tidb:v8.5.1` 镜像 | **已同意** |

「同意 2」之前为什么跑不动：Claude 2026-10-15 只读核对了本机模块缓存（`C:\Users\luyua\go\pkg\mod\cache\download`），下面这些依赖只有 `.info` / `.mod`、没有源码 `.zip`，`GOPROXY=off` 时编译不了。装 Go 不会补它们。

| 用在哪 | 缺源码的依赖 |
|---|---|
| go/db（`0aa1027bc`、`73b852f32` 与当前 HEAD 都要）| `github.com/alicebob/miniredis/v2 v2.36.1` |
| proto2mysql | `github.com/go-sql-driver/mysql v1.8.1` |
| proto2mysql/tools/proto2sql | `github.com/bufbuild/protocompile v0.14.1`、`github.com/go-sql-driver/mysql v1.8.1`、`golang.org/x/sync v0.9.0` |

go/login 在这两个提交上用 miniredis v2.35.0，缓存里有。`github.com/coreos/go-systemd/v22` 与 `github.com/prometheus/procfs` 也缺，但它们只在 Linux 上参与编译，Windows 上跑不到。核对按的是 go.mod 的 require 清单，以实际运行为准。
| 同意 3 | 拉取 `mysql:8.4` 与 `pingcap/tidb:v8.5.1` 镜像 | **已同意** |

---

## 1. 第 0 步：环境与探针（先做，原样贴回输出）

```powershell
# 环境变量先设,探针最后再打印一次,证明后续步骤处在什么环境
if (-not (Get-Command go -ErrorAction SilentlyContinue)) { $env:PATH = 'C:\Program Files\Go\bin;' + $env:PATH }
$env:GOTOOLCHAIN = 'local'          # 本地 Go 版本不够时直接报错,不静默下载工具链
$env:GOFLAGS     = '-mod=readonly'
$env:GOPROXY     = 'off'            # 「同意 2」之前不联网

git -C D:\luyuan\xuanming-server-mmo-cpp rev-parse HEAD
git -C D:\luyuan\xuanming-server-mmo-cpp cat-file -e '0aa1027bc^{commit}'; "0aa1027bc exists: $LASTEXITCODE"
git -C D:\luyuan\xuanming-server-mmo-cpp cat-file -e '73b852f32^{commit}'; "73b852f32 exists: $LASTEXITCODE"
git -C D:\luyuan\xuanming-server-mmo-cpp worktree list
$PSVersionTable.PSVersion
Get-Command pwsh, go, gcc, docker -ErrorAction SilentlyContinue | Format-Table Name, Source
go version
docker version --format '{{.Server.Version}}'
git -C D:\luyuan\proto2mysql rev-parse HEAD
git -C D:\luyuan\proto2mysql status -sb
go env GOVERSION GOROOT GOMODCACHE GOPROXY GOTOOLCHAIN GOFLAGS CGO_ENABLED
```

判定：

- 两个 `cat-file` 都应返回 0；任一失败就停下。主仓库 HEAD 不是 `0aa1027bc` 是预期的，照实记录即可。
- proto2mysql 应在分支 `feat/conditional-save`，HEAD 以 `e81a1e7` 开头，工作区干净。
- `go version` 应 ≥ 1.26.5（`0aa1027bc` 上 go/db 的 go.mod 要求）。
- `docker version` 连不上守护进程：任务 B 的真库与竞态两项记「未执行」，不代为启动 Docker Desktop。
- `pwsh` 不在 PATH 上：把 Codex 自带的 pwsh 所在目录临时加进 `$env:PATH`（只改本进程）。版本需 ≥ 7.3。

---

## 2. 任务 A：主仓库第 0 批

### 2.1 建三个临时 worktree

路径要短（本机没有开长路径支持）。先清掉本单上一轮可能留下的同名 worktree（只处理这三个路径）：

```powershell
foreach ($p in 'godb_head','godb_base','godb_iso') {
  $path = Join-Path $env:TEMP $p
  if ((git -C D:\luyuan\xuanming-server-mmo-cpp worktree list) -match [regex]::Escape($path)) {
    git -C D:\luyuan\xuanming-server-mmo-cpp worktree remove --force $path
  }
}
git -C D:\luyuan\xuanming-server-mmo-cpp worktree prune
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach $env:TEMP\godb_head 0aa1027bc
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach $env:TEMP\godb_base 73b852f32
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach $env:TEMP\godb_iso  73b852f32
```

- `godb_head`：被验证的提交。
- `godb_base`：改动之前的基线。
- `godb_iso`：基线上只拷入本批的四个文件（§2.4）。这两个源文件相对 `73b852f32` 只有本批的改动（key_ordered_consumer.go +22/-1、key_ordered_producer.go +11/-2），所以隔离是干净的。

### 2.2 基线（在 `godb_base` 里跑，必做）

```powershell
Push-Location $env:TEMP\godb_base\go\db
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\base_b.txt; "exit=$LASTEXITCODE"
Pop-Location
Push-Location $env:TEMP\godb_base\go\login
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\base_d.txt; "exit=$LASTEXITCODE"
Pop-Location
Select-String -Path $env:TEMP\base_b.txt, $env:TEMP\base_d.txt -Pattern 'could not start miniredis|listen tcp|bind:|connectex|forbidden'
```

- 最后一条有任何命中：按环境问题（回环被拦）停下报告；放行后重跑本节，这一次不算 §0 的连续重试。不要带着这样的基线往下跑。
- 记录每个包的顶层用例数、PASS / FAIL / SKIP 数和失败用例名。预期顶层用例数：go/db 72 个，go/login 7 个。基线本身有失败不算本单的失败。

### 2.3 login 自锁的「红」证据（在 `godb_base` 里跑）

```powershell
Copy-Item $env:TEMP\godb_head\go\login\internal\kafka\key_ordered_producer_lock_test.go `
          $env:TEMP\godb_base\go\login\internal\kafka\
Push-Location $env:TEMP\godb_base\go\login
go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v *> $env:TEMP\base_c_red.txt; "exit=$LASTEXITCODE"
Pop-Location
```

预期：**失败**，约 5 秒后输出里有 `SendTask deadlocked on p.mu after a partition-unavailable error`，另有一行 mock 的 `Expected to exhaust all expectations, but 1 are left.`（属预期）。如果它通过了，或者以 `precondition:` 开头的文案失败，停下报告。

### 2.4 隔离运行（在 `godb_iso` 里跑：基线 + 只有本批的四个文件）

```powershell
foreach ($f in 'go\db\internal\kafka\key_ordered_consumer.go',
               'go\db\internal\kafka\subshard_route_test.go',
               'go\login\internal\kafka\key_ordered_producer.go',
               'go\login\internal\kafka\key_ordered_producer_lock_test.go') {
  Copy-Item (Join-Path $env:TEMP\godb_head $f) (Join-Path $env:TEMP\godb_iso $f) -Force
}
Push-Location $env:TEMP\godb_iso\go\db
go test ./internal/kafka/ -run 'TestSpreadKey|TestSubShardIndex|TestRouteToSubShard' -count=1 -v *> $env:TEMP\iso_a.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\iso_b.txt; "exit=$LASTEXITCODE"
Pop-Location
Push-Location $env:TEMP\godb_iso\go\login
go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v *> $env:TEMP\iso_c.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\iso_d.txt; "exit=$LASTEXITCODE"
Pop-Location
```

预期顶层用例数：iso-a 5、iso-b 77、iso-c 1、iso-d 8。这一组给出的是「本批代码本身」的结论。

### 2.5 被验证的提交（在 `godb_head` 里跑）

每条命令单独记录退出码。

```powershell
Push-Location $env:TEMP\godb_head\go\db
go test ./internal/kafka/ -run 'TestSpreadKey|TestSubShardIndex|TestRouteToSubShard' -count=1 -v *> $env:TEMP\head_a.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\head_b.txt; "exit=$LASTEXITCODE"
Pop-Location

Push-Location $env:TEMP\godb_head\go\login
go vet ./internal/kafka/ *> $env:TEMP\head_c_vet.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v *> $env:TEMP\head_c.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\head_d.txt; "exit=$LASTEXITCODE"
Pop-Location

Push-Location $env:TEMP\godb_head
pwsh -NoProfile -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1 *> $env:TEMP\head_e.txt; "exit=$LASTEXITCODE"
pwsh -NoProfile -File tools/scripts/test_stress_summarize.ps1 *> $env:TEMP\head_f.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准：

| 步 | 通过标准 |
|---|---|
| a | 退出码 0；5 行顶层 `--- PASS`；0 个 SKIP。a 与 c 不依赖回环网络 |
| b | 顶层用例 77 个；FAIL 集合是 base-b FAIL 集合的子集。退出码照实记录，由基线同名失败造成的非 0 不判失败 |
| c | vet 退出码 0；test 退出码 0，有 `--- PASS: TestSendTaskLeaderNotAvailableDoesNotDeadlock` |
| d | 顶层用例 8 个；FAIL 集合是 base-d FAIL 集合的子集（口径同 b）|
| e | 退出码 0；7 行 `[PASS]`；汇总行 `total=7 pass=7 fail=0` |
| f | 退出码 0；末行 `PASS`；`ok:` 行里出现 `4.0ms`、`3.0ms`、`0.5ms`、`0.7ms`、`12.5ms`、`9.0ms` 六个平均值 |

只在对应步骤非 0 时补做的对照：

- c 的 vet 非 0：在 `godb_base\go\login` 跑一次 `go vet ./internal/kafka/`，记为 base-vet。
- f 失败且失败点不是本批新增的两段断言（`t18m_db` 那一行与六个平均值）：在 `godb_base` 根目录跑同一条 f 命令，记为 base-f。

归因规则：

- 编译失败先看第一条 error 的文件路径。属于本批：`go/db/internal/kafka/key_ordered_consumer.go` 里的 `subShardIndex` / `spreadKey`、`subshard_route_test.go`；`go/login/internal/kafka/key_ordered_producer.go` 里的 `markPartitionUnavailableLocked`、`key_ordered_producer_lock_test.go`。属于同一提交里的其它会话：`go/db/internal/config`、`go/db/internal/metrics`、`go/db/internal/logic/pkg/proto_sql`、`go/login/internal/config`、`go/shared/sqlpoolstats`、`go/shared/kafkautil`、`go/shared/placement`。
- b / d 相对基线新增的运行期失败：看 iso-b / iso-d 是否同样失败。同样失败 = 本批的问题；只在 head 上失败 = 同一提交里其它会话的问题。
- 两类分开汇报。报 `no required module provides package github.com/IBM/sarama/mocks` 或 go.sum 缺项时停下报告，不要自己修。
- f 断言失败时，脚本会先自行删除 `%TEMP%\stress-summarize-test-<guid>` 再退出，取证以 head_f.txt 里打印的 stress_summarize 全量输出为准；只有脚本抛异常（没走到断言）时那个目录才会留下，这种情况保留并在回报里写出路径。f 会在 worktree 下创建被 `.gitignore` 忽略的 `run\logs\cpp_nodes`，在临时 worktree 里无害。

### 2.6 收尾（失败时也要做）

```powershell
git -C $env:TEMP\godb_head status --porcelain                    # 应为空
git -C $env:TEMP\godb_head diff --stat -- '*go.mod' '*go.sum'      # 应为空
foreach ($p in 'godb_base','godb_iso','godb_head') {
  git -C D:\luyuan\xuanming-server-mmo-cpp worktree remove --force (Join-Path $env:TEMP $p)
}
git -C D:\luyuan\xuanming-server-mmo-cpp worktree prune
```

结果文件 `%TEMP%\base_*.txt`、`iso_*.txt`、`head_*.txt` 保留到负责人确认收到回报。

---

## 3. 任务 B：proto2mysql `SaveConditional`

工作目录 `D:\luyuan\proto2mysql`，分支 `feat/conditional-save`。这个仓库工作区干净，直接在里面跑。

### 3.1 离线部分（不需要数据库）

```powershell
Push-Location D:\luyuan\proto2mysql
go vet ./... *> $env:TEMP\p2m_vet.txt; "exit=$LASTEXITCODE"
go test -count=1 ./... *> $env:TEMP\p2m_test.txt; "exit=$LASTEXITCODE"
Pop-Location
Push-Location D:\luyuan\proto2mysql\tools\proto2sql      # 独立 module
go vet ./... *> $env:TEMP\p2m_tool_vet.txt; "exit=$LASTEXITCODE"
go test -count=1 ./... *> $env:TEMP\p2m_tool_test.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准：四条退出码都是 0。任一条非 0 时，在 proto2mysql 的 main（`588c308`，即 v0.2.0）建临时 worktree 复跑同一条命令对比失败集合，跑完删除那个 worktree。

### 3.2 起库（需要「同意 3」，已获）

参数出处是 proto2mysql 的 `docs/testing.md` 第 23–38 行；那里是 bash 写法，下面是等价的 pwsh 单行。`<口令>` 用 `docs/testing.md` 第 28 行的值。

```powershell
docker ps -a --filter name=proto2mysql-          # 有同名容器就停下报告,不要删(可能是上一轮或别人的)
docker run -d --name proto2mysql-it -p 13307:3306 -e MYSQL_ROOT_PASSWORD=<口令> -e MYSQL_DATABASE=proto2mysql_test mysql:8.4 '--sql-mode=STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'
docker run -d --name proto2mysql-tidb -p 14000:4000 pingcap/tidb:v8.5.1
```

就绪判据（轮询等待，不算 §0 禁止的连续重试，间隔 3 秒、最多 40 次）：

- MySQL：`docker logs proto2mysql-it 2>&1 | Select-String "ready for connections" | Select-String "port: 3306"` 有命中。初始化阶段的临时实例也会打印 `ready for connections`，但端口是 0，所以必须同时匹配 `port: 3306`。
- TiDB：下面这条成功返回即视为就绪（借 MySQL 容器里的客户端，本机没有 mysql 客户端），同时完成建库并取到版本与 `tidb_retry_limit`：

```powershell
docker exec proto2mysql-it mysql -h host.docker.internal -P 14000 -uroot -e "CREATE DATABASE IF NOT EXISTS proto2mysql_test; SELECT VERSION(), @@tidb_retry_limit;"
docker exec proto2mysql-it mysql -uroot -p<口令> -e "SELECT VERSION();"
```

- MySQL 容器靠 `MYSQL_DATABASE` 自动建库；**TiDB 不会建库**，上面那条 `CREATE DATABASE` 必须成功，否则用例是 FAIL 不是 SKIP。

### 3.3 真库用例

**必须用环境变量传 DSN**。不要照抄 `docs/release-notes-v0.3.0.md` 第 55–56 行那两条不带 DSN 的命令：不带 DSN 时用例会回落到已入库的 `testdata/db.json`（127.0.0.1:3307、库 `game`），并在那里建表删表。不要修改 `testdata/db.json`。

```powershell
Push-Location D:\luyuan\proto2mysql
$env:PROTO2MYSQL_INTEGRATION = '1'
$env:PROTO2MYSQL_TEST_DSN = 'root:<口令>@tcp(127.0.0.1:13307)/proto2mysql_test'
go test -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_it_mysql.txt; "exit=$LASTEXITCODE"
$env:PROTO2MYSQL_TEST_DSN = 'root:@tcp(127.0.0.1:14000)/proto2mysql_test'
go test -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_it_tidb.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准（两份结果文件各自满足）：

- 退出码 0。
- `(Select-String -Path <结果文件> -Pattern '--- SKIP').Count` 为 0（这个模式连缩进的子用例行一起数）。
- 名字含 `SaveConditional` 的用例全部 PASS；只读核对时数到 23 个离线加 4 个真库，以实际输出为准，把数字写进回报。
- TiDB 的 `tidb_retry_limit` 大于 0，并把值写进回报。

### 3.4 竞态检测

`-race` 需要 `CGO_ENABLED=1` 和 gcc。本机没有 gcc，也没有安装 WSL 发行版。做法二选一：

- **有 gcc 的环境**：用下面的完整代码块（自己重设变量，不依赖 3.3）。
- **没有 gcc**：用 `golang:1.26.5` 镜像的容器跑，只读挂载 `D:\luyuan\proto2mysql`，用 `-e` 显式传 `CGO_ENABLED=1`、`GOFLAGS=-mod=readonly`、`GOPROXY`、`PROTO2MYSQL_INTEGRATION=1` 与 `PROTO2MYSQL_TEST_DSN`（主机写 `host.docker.internal:13307`）。这需要额外拉 `golang:1.26.5` 镜像并在容器里下载模块，**先向负责人请示**（不在已获的「同意 3」范围内）。
- 两者都不行：写「race 未执行」，不安装 gcc 或 WSL。

```powershell
Push-Location D:\luyuan\proto2mysql
$env:CGO_ENABLED = '1'
$env:PROTO2MYSQL_INTEGRATION = '1'
$env:PROTO2MYSQL_TEST_DSN = 'root:<口令>@tcp(127.0.0.1:13307)/proto2mysql_test'
go test -race -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_race.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准（三条同时满足，否则只能记「race 未覆盖并发用例」）：

- 退出码 0，且没有 `WARNING: DATA RACE`。
- `--- SKIP` 计数为 0。
- 出现两行子用例 PASS：`--- PASS: TestSaveConditionalRealDatabaseConcurrentWritersConvergeToNewest/preexisting=false` 与 `/preexisting=true`。没带库时父用例照样打印 PASS，所以只看父用例证明不了并发用例跑过。

### 3.5 收尾（失败时也要做）

```powershell
docker rm -f proto2mysql-it proto2mysql-tidb
git -C D:\luyuan\proto2mysql status --porcelain     # 应为空
```

只删这两个容器；**不要执行** `docs/testing.md` 里的 `docker compose -f tidb-cluster.yml down -v`（本单没有起集群）。

---

## 4. 不在本单范围内

- 主仓库 HEAD 上 2026-10-14 新增的 L-04 第 1 批代码（未提交）。它的验证命令在 [godb-pipeline-launch-todo.md](../ops/godb-pipeline-launch-todo.md) §2，另行执行。

---

## 5. 失败时保留什么

- 第 0 步探针的完整输出。
- 每条命令的退出码与对应的结果文件。
- 编译错误：第一条 error 及其前后 20 行（要带文件路径）。
- 失败用例：完整的 `go test -v` 输出。整包失败时允许用 `-run '^用例名$' -v` 单独补跑**一次**取证，不循环重试。
- 2.3 的红色运行：含 `deadlocked` 的全文。
- e、f 的完整控制台输出。
- 收尾时的 `git status --porcelain` 与 `go.mod` / `go.sum` 的 diff。

---

## 6. 回报表（填好后交给负责人）

| 项 | 结果（通过 / 失败 / 未执行）| 退出码 | 关键数字 | 备注 |
|---|---|---|---|---|
| 第 0 步 | | — | Go 版本、pwsh 版本、Docker 版本、平台、设置后的 go env 三项 | |
| 同意 2 | 已获 / 未获 | — | — | 若中途请示过,写明时间 |
| base-b | | | 顶层用例数、FAIL 数、SKIP 数 | 失败用例名 |
| base-d | | | 同上 | |
| 2.3 红 | 预期为失败 | | 耗时 | 是否出现 `deadlocked` |
| iso-a / iso-b / iso-c / iso-d | | | 顶层用例数（应为 5 / 77 / 1 / 8）| 本批代码本身的结论 |
| a | | | PASS 数 | |
| b | | | 顶层用例数；相对 base-b 新增的失败；与基线同名的失败 | 新增失败在 iso-b 上是否也失败 |
| c（vet / test）| | | | |
| d | | | 顶层用例数；相对 base-d 新增的失败；与基线同名的失败 | 新增失败在 iso-d 上是否也失败 |
| e | | | `total / pass / fail` | |
| f | | | 六个平均值是否齐 | |
| p2m 离线四条 | | | | 非 0 时与 main 的对比结果 |
| p2m 真库 MySQL | | | PASS / SKIP 数 | MySQL 版本 |
| p2m 真库 TiDB | | | PASS / SKIP 数、`tidb_retry_limit` | TiDB 版本 |
| p2m race | | | 两个并发子用例是否 PASS | 平台；是否带真库 |
| 收尾检查 | | — | — | worktree 与容器是否已删 |

结论分三段写：基线 → 基线加本批（iso）→ 被验证的提交（head）。

---

## 7. 结果回来之后（Claude 做）

- `docs/PROGRESS.md` 追加「补充:存盘管线第 0 批与 proto2mysql 的 Codex 复验结果」。
- [db-task-pipeline-bottlenecks.md](../design/db-task-pipeline-bottlenecks.md) §7 的表加「验证」列。
- proto2mysql 的结果写进它的 `docs/release-notes-v0.3.0.md`「验证」节，在 `feat/conditional-save` 分支上提交并推送（负责人 2026-10-13 同意沿用上一会话的授权，仅限功能分支）。全绿后合入 main（负责人已同意合入；但推送授权只到功能分支，推送 main 之前再向负责人确认一次），不打 tag（负责人已定）。
- [上线待做](../ops/godb-pipeline-launch-todo.md) §1 的状态同步更新。
