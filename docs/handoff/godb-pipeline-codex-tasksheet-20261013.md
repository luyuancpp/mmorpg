# Codex 任务单：存盘管线第 0 批与 proto2mysql 条件写原语的验证（2026-10-13）

> **谁执行**：Codex。Claude 不编译、不测试（AGENTS.md §10.1）。
> **验证什么**：[交接](godb-pipeline-handoff-20261013.md) §3.1（主仓库第 0 批）与 §3.2（proto2mysql `SaveConditional`）。两者都从未编译、未运行。
> **结果交给谁**：把 §5 的回报表填好交给负责人，由负责人贴回 Claude 的对话。Claude 按交接 §3.0 写回文档。
> **本单相对交接 §3 的订正**（依据是 2026-10-13 的只读核对）：
> - 验证对象是 `0aa1027bc`，不是 `d5cbb8a8e`。两者代码逐字节相同，`0aa1027bc` 只多改了 4 个文档。
> - 交接里的六条命令不能照抄顺序执行：每行都以相对路径 `cd go/db` 开头，第二行就会失败。本单每条命令各自写明工作目录。
> - 主工作区现在有另一个会话的未提交改动，其中 `go/shared/sqlpoolstats/collector.go` 在 go/db 的编译闭包里。所以**不在主工作区跑**，改在干净的临时 worktree 里跑。
> - go/db、go/login 这两个包此前没有任何运行记录，「无新增失败」必须靠基线对比得出，本单把基线列为必做。
> - `test_stress_summarize.ps1` 没有 `#requires -Version 7`；它在内部按名字再起一个 `pwsh`，所以 `pwsh` 必须在 PATH 上。

---

## 0. 规则

- 只执行本单列出的命令。任何一步失败就保留输出后停下，**不连续重试、不改断言、不跳过用例、不改代码**。
- 不 commit、不 push、不打 tag；不改任何 `go.mod` / `go.sum`，不运行 `go mod tidy`、`go get`、`go env -w`。
- 不碰主工作区 `D:\luyuan\xuanming-server-mmo-cpp` 里的文件（那里有别的会话在写）。不复用、不清理已有的 worktree `C:\Users\luyua\.codex\worktrees\f452\…`（它停在 `73b852f32` 且带别人的未提交改动）。
- Shell 用 PowerShell 7（`pwsh`）。不要用 Windows PowerShell 5.1 或 cmd：`&&` 在 5.1 是语法错误，`-run 'A|B'` 在 cmd 下会被当成管道。
- 被 Windows Application Control 拦截测试 exe 时（事件 3077 / 3033），记录事件号与被拦路径后停下，不绕过。

### 0.1 需要负责人事先同意的三件事

没有得到明确同意的，对应步骤不执行，在回报表里写「未获同意，未执行」。

| # | 事项 | 用在哪 |
|---|---|---|
| 同意 1 | 本机若没有 Go 1.26.5，是否安装 | 任务 A 的 a–d、任务 B 全部 |
| 同意 2 | 是否允许联网下载 Go 模块（建议 `GOPROXY=https://goproxy.cn,direct`）。核对时本机没有模块缓存，`go.mod` / `go.sum` 虽然没变，首次执行仍要下载全部依赖 | 同上 |
| 同意 3 | 是否允许拉取 `mysql:8.4` 与 `pingcap/tidb:v8.5.1` 镜像 | 任务 B 的真库用例 |

---

## 1. 第 0 步：环境探针（先做，原样贴回输出）

```powershell
git -C D:\luyuan\xuanming-server-mmo-cpp rev-parse HEAD
git -C D:\luyuan\xuanming-server-mmo-cpp worktree list
$PSVersionTable.PSVersion
Get-Command pwsh, go, gcc, docker -ErrorAction SilentlyContinue | Format-Table Name, Source
go version
go env GOVERSION GOROOT GOMODCACHE GOPROXY GOTOOLCHAIN GOFLAGS CGO_ENABLED
git -C D:\luyuan\proto2mysql rev-parse HEAD
git -C D:\luyuan\proto2mysql status -sb
```

判定：

- 主仓库 HEAD 应为 `0aa1027bc901efa02b6d264068b53dd56252de41`。不是就停下报告。
- proto2mysql 应在分支 `feat/conditional-save`，HEAD 以 `e81a1e7` 开头，工作区干净。
- `go` 找不到，或版本低于 1.26.5：需要「同意 1」。
- `pwsh` 不在 PATH 上：把 Codex 自带的 pwsh 所在目录临时加进本进程的 PATH（只改 `$env:PATH`，不改系统设置）。版本需 ≥ 7.3。

本进程的环境变量（只在当前进程设置）：

```powershell
$env:GOTOOLCHAIN = 'local'          # 本地 Go 版本不够时直接报错，不静默下载工具链
$env:GOFLAGS     = '-mod=readonly'
$env:GOPROXY     = 'off'            # 先不联网；报缺模块即停下，获得「同意 2」后改成 https://goproxy.cn,direct
```

---

## 2. 任务 A：主仓库第 0 批

### 2.1 建两个临时 worktree

路径要短（本机没有开长路径支持）。

```powershell
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach $env:TEMP\godb_head 0aa1027bc
git -C D:\luyuan\xuanming-server-mmo-cpp worktree add --detach $env:TEMP\godb_base 73b852f32
```

- `godb_head`：被验证的代码。
- `godb_base`：改动之前的基线。

### 2.2 基线（在 `godb_base` 里跑，必做）

```powershell
# base-b：go/db 的 kafka 包
Push-Location $env:TEMP\godb_base\go\db
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\base_b.txt; "exit=$LASTEXITCODE"
Pop-Location

# base-d：go/login 的 kafka 包
Push-Location $env:TEMP\godb_base\go\login
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\base_d.txt; "exit=$LASTEXITCODE"
Pop-Location
```

记录每个包的顶层用例数、PASS / FAIL / SKIP 数和失败用例名。预期顶层用例数：go/db 72 个，go/login 7 个。基线本身有失败不算本单的失败，如实记录即可。

### 2.3 login 自锁的「红」证据（在 `godb_base` 里跑）

只把新测试文件拷进基线，不拷修复：

```powershell
Copy-Item $env:TEMP\godb_head\go\login\internal\kafka\key_ordered_producer_lock_test.go `
          $env:TEMP\godb_base\go\login\internal\kafka\
Push-Location $env:TEMP\godb_base\go\login
go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v *> $env:TEMP\base_c_red.txt; "exit=$LASTEXITCODE"
Pop-Location
```

预期：**失败**，约 5 秒后输出里有 `SendTask deadlocked on p.mu after a partition-unavailable error`，另有一行 mock 的 `Expected to exhaust all expectations, but 1 are left.`（属预期）。如果它通过了，或者以 `precondition:` 开头的文案失败，停下报告。

### 2.4 被验证的代码（在 `godb_head` 里跑）

每条命令单独记录退出码。

```powershell
# a：go/db 新用例
Push-Location $env:TEMP\godb_head\go\db
go test ./internal/kafka/ -run 'TestSpreadKey|TestSubShardIndex|TestRouteToSubShard' -count=1 -v *> $env:TEMP\head_a.txt; "exit=$LASTEXITCODE"

# b：go/db 整包
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\head_b.txt; "exit=$LASTEXITCODE"
Pop-Location

# c：go/login 回归用例（vet 与 test 分开跑、分开记退出码）
Push-Location $env:TEMP\godb_head\go\login
go vet ./internal/kafka/ *> $env:TEMP\head_c_vet.txt; "exit=$LASTEXITCODE"
go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v *> $env:TEMP\head_c.txt; "exit=$LASTEXITCODE"

# d：go/login 整包
go test ./internal/kafka/... -count=1 -v *> $env:TEMP\head_d.txt; "exit=$LASTEXITCODE"
Pop-Location

# e、f：两个脚本测试，从 worktree 根执行
Push-Location $env:TEMP\godb_head
pwsh -NoProfile -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1 *> $env:TEMP\head_e.txt; "exit=$LASTEXITCODE"
pwsh -NoProfile -File tools/scripts/test_stress_summarize.ps1 *> $env:TEMP\head_f.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准：

| 步 | 通过标准 |
|---|---|
| a | 退出码 0；5 行顶层 `--- PASS`（`TestSpreadKeyMatchesSplitMix64Reference`、`TestSubShardIndexIsStablePerKey`、`TestSubShardIndexSpreadsBwmarrinIDsWithZeroStep`、`TestSubShardIndexSpreadsSequentialIDs`、`TestRouteToSubShardUsesSpreadIndex`）；0 个 SKIP |
| b | 退出码 0；顶层用例 77 个；失败用例集合不比 base-b 多 |
| c | vet 退出码 0；test 退出码 0，有 `--- PASS: TestSendTaskLeaderNotAvailableDoesNotDeadlock` |
| d | 退出码 0；顶层用例 8 个；失败用例集合不比 base-d 多 |
| e | 退出码 0；7 行 `[PASS]`；汇总行 `total=7 pass=7 fail=0` |
| f | 退出码 0；末行 `PASS`；`ok:` 行里出现 `4.0ms`、`3.0ms`、`0.5ms`、`0.7ms`、`12.5ms`、`9.0ms` 六个平均值 |

归因规则（a–d 编译失败时）：先看第一条 error 的文件路径。

- 属于本批：`go/db/internal/kafka/key_ordered_consumer.go` 里的 `subShardIndex` / `spreadKey`、`subshard_route_test.go`；`go/login/internal/kafka/key_ordered_producer.go` 里的 `markPartitionUnavailableLocked`、`key_ordered_producer_lock_test.go`。
- 属于其它会话（同在 `d5cbb8a8e` 里，同样从未编译）：`go/db/internal/config`、`go/db/internal/metrics`、`go/db/internal/logic/pkg/proto_sql`、`go/login/internal/config`、`go/shared/sqlpoolstats`、`go/shared/kafkautil`、`go/shared/placement`。
- 两类分开汇报。报 `no required module provides package github.com/IBM/sarama/mocks` 或 go.sum 缺项时停下报告，不要自己修。

说明：f 会在 worktree 下创建被 `.gitignore` 忽略的 `run\logs\cpp_nodes`，在临时 worktree 里无害。f 中途失败时保留 `%TEMP%\stress-summarize-test-<guid>` 目录。

### 2.5 收尾

```powershell
git -C $env:TEMP\godb_head status --porcelain          # 应为空
git -C $env:TEMP\godb_head diff --stat -- '*go.mod' '*go.sum'   # 应为空
git -C D:\luyuan\xuanming-server-mmo-cpp worktree remove --force $env:TEMP\godb_base
git -C D:\luyuan\xuanming-server-mmo-cpp worktree remove --force $env:TEMP\godb_head
```

`godb_base` 里拷进去的那个测试文件随 worktree 一起删除。结果文件 `%TEMP%\base_*.txt`、`%TEMP%\head_*.txt` 保留到负责人确认收到回报。

---

## 3. 任务 B：proto2mysql `SaveConditional`

工作目录 `D:\luyuan\proto2mysql`，分支 `feat/conditional-save`。这个仓库工作区干净，直接在里面跑，不建 worktree。

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

通过标准：四条退出码都是 0；既有用例没有新增失败。

### 3.2 真库部分（需要「同意 3」）

起库按 proto2mysql 的 `docs/testing.md` 第 23–38 行（容器名、端口、root 口令、就绪判据都以那里为准）：MySQL 8.4 映射到 13307，TiDB v8.5.1 映射到 14000。

- MySQL 容器靠 `MYSQL_DATABASE=proto2mysql_test` 自动建库。
- **TiDB 容器不会建库**，要手工执行一次 `CREATE DATABASE proto2mysql_test;`。不建库时用例是 FAIL，不是 SKIP。
- **必须用环境变量传 DSN。** 不要照抄 `docs/release-notes-v0.3.0.md` 第 55–56 行那两条不带 DSN 的命令：不带 DSN 时用例会回落到已入库的 `testdata/db.json`（127.0.0.1:3307、库 `game`），并在那里建表删表。不要修改 `testdata/db.json`。

```powershell
Push-Location D:\luyuan\proto2mysql
$env:PROTO2MYSQL_INTEGRATION = '1'

# MySQL 8.4（<口令> 用 docs/testing.md 第 28 行的值）
$env:PROTO2MYSQL_TEST_DSN = 'root:<口令>@tcp(127.0.0.1:13307)/proto2mysql_test'
go test -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_it_mysql.txt; "exit=$LASTEXITCODE"

# TiDB 8.5.1（root 无口令）
$env:PROTO2MYSQL_TEST_DSN = 'root:@tcp(127.0.0.1:14000)/proto2mysql_test'
go test -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_it_tidb.txt; "exit=$LASTEXITCODE"
Pop-Location
```

通过标准：两条退出码都是 0；`-v` 输出里**没有** `SKIP: TestSaveConditional`；名字含 `SaveConditional` 的用例全部 PASS（只读核对时数到 23 个离线加 4 个真库，以实际输出为准，把数字写进回报）。TiDB 那一条另外确认 `SELECT @@tidb_retry_limit;` 大于 0，并把值写进回报。

### 3.3 竞态检测

`-race` 需要 `CGO_ENABLED=1` 和 gcc。本机没有 gcc 时在 WSL 或 Linux 上跑，并在回报里写明平台。

```powershell
# 环境变量沿用 3.2 的 MySQL 一组（带上真库，唯一的多协程用例才会被检测到）
go test -race -count=1 -run SaveConditional -v . *> $env:TEMP\p2m_race.txt; "exit=$LASTEXITCODE"
```

通过标准：退出码 0，没有 `WARNING: DATA RACE`。如果只能不带数据库跑，如实写「race 只覆盖离线用例，4 个真库用例 SKIP」。

### 3.4 收尾

按 `docs/testing.md` 第 268–271 行删除两个测试容器。`git -C D:\luyuan\proto2mysql status --porcelain` 应为空。

---

## 4. 失败时保留什么

- 第 0 步探针的完整输出。
- 每条命令的退出码与对应的结果文件。
- 编译错误：第一条 error 及其前后 20 行（要带文件路径）。
- 失败用例：完整的 `go test -v` 输出。整包失败时允许用 `-run '^用例名$' -v` 单独补跑**一次**取证，不循环重试。
- 2.3 的红色运行：含 `deadlocked` 的全文。
- e、f 的完整控制台输出。
- 收尾时的 `git status --porcelain` 与 `go.mod` / `go.sum` 的 diff。

---

## 5. 回报表（填好后交给负责人）

| 项 | 结果（通过 / 失败 / 未执行）| 退出码 | 关键数字 | 备注 |
|---|---|---|---|---|
| 第 0 步 | | — | Go 版本、pwsh 版本、平台 | |
| 同意 1 / 2 / 3 | 已获 / 未获 | — | — | |
| base-b | | | 顶层用例数、FAIL 数、SKIP 数 | 失败用例名 |
| base-d | | | 同上 | |
| 2.3 红 | 预期为失败 | | 耗时 | 是否出现 `deadlocked` |
| a | | | PASS 数 | |
| b | | | 顶层用例数；相对 base-b 新增的失败 | 首个报错文件属于哪一类 |
| c（vet / test）| | | | |
| d | | | 顶层用例数；相对 base-d 新增的失败 | |
| e | | | `total / pass / fail` | |
| f | | | 六个平均值是否齐 | |
| p2m 离线四条 | | | | |
| p2m 真库 MySQL | | | PASS / SKIP 数 | MySQL 版本 |
| p2m 真库 TiDB | | | PASS / SKIP 数、`tidb_retry_limit` | TiDB 版本 |
| p2m race | | | | 平台；是否带真库 |
| 收尾检查 | | — | — | worktree 与容器是否已删 |

---

## 6. 结果回来之后（Claude 做）

- `docs/PROGRESS.md` 追加「补充:存盘管线第 0 批与 proto2mysql 的 Codex 复验结果」。
- [db-task-pipeline-bottlenecks.md](../design/db-task-pipeline-bottlenecks.md) §7 的表加「验证」列。
- proto2mysql 的结果写进它的 `docs/release-notes-v0.3.0.md`「验证」节。是否提交、是否合入 main、是否打 `v0.3.0`，分别等负责人答复。
