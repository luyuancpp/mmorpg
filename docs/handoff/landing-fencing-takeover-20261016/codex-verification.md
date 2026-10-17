# 给 Codex 的验证命令汇总（2026-10-16 接手批次，全部未编译、未测试）
> 由主线从各实现 / 修复代理的交付说明原样汇总。顺序：先客户端与 robot（与 a–e 无依赖），再按 [scene-enter-chain-residuals.md](../../design/scene-enter-chain-residuals.md) §8 做 a–e：一次 proto 生成 → 第一轮复现红 → 第二轮全绿。C++ MSBuild 一律串行 /m:1。失败时保留首个失败的完整输出，不连续自动重试。
> **生成基线**：W1 / W2 的 proto 改动已经在工作区，§8 的「改 proto 前先生成一次」不能在主工作区做；按 W2 的做法在临时 worktree 里去掉本批新增行生成基线，或降级为按清单人工核对，并把「基线比对未执行」列为未验证项。**不要为取基线改写共享工作区的 proto。**

## 1. 客户端（D:\luyuan\mmorpg-client）

以下都还没编译、没测试，等 Codex 验证。

一、客户端（工作目录 D:\luyuan\mmorpg-client）
1. 编译检查：pwsh -File tools/client_compile_check.ps1
   通过标准：0 错误。若错误不在 LandingSyncClient.cs、GuildClient.cs 的 DrainSync 段、SyncDigestStore.cs 或两个测试文件里，可能来自别的会话在途的 GuildClient、Team、Mail 等改动，保留原文另行报告，不要修。
2. Unity EditMode（先关闭打开该工程的 Unity 编辑器）：
   "<Unity.exe>" -batchmode -nographics -projectPath D:\luyuan\mmorpg-client -runTests -testPlatform EditMode -testFilter "LandingSyncClient|GuildClientTests|GuildUi" -testResults D:\luyuan\mmorpg-client\Logs\editmode-landing-guild.xml -logFile D:\luyuan\mmorpg-client\Logs\editmode-landing-guild.log
   注意：GuildUiTests.cs 里的类名是 GuildClientTests（R-2 新用例就在其中），只写 "GuildUi" 匹配不到它，所以过滤里加了 GuildClientTests。
   重点用例：
   - Guild_ApplicationPulledOnlyWhenNotInGuild(True/False)
   - Guild_InGuildSecondLandingWithUnchangedDigest_SendsNoGuildRequest
   - Guild_EconomyAckDoesNotPairReadsFromDifferentBatches
   - Guild_EconomyPageReadSentBeforeDigestDoesNotCount
   - Guild_NestedSynchronousReplyFiresInnerAndOuterAckOnceEach
   - Guild_QueuedSyncReadIsNotSentWhileBusyAndIsSentOnceIdle
   - BackgroundMyApplicationsPull_ThenOpeningWindowSendsOnlyGetPlayerGuild
   通过标准：原有用例数不减、全绿。失败时保留 results xml 中失败用例的消息与栈，以及 log 里的编译错误段。



## 2. robot sync-smoke

二、robot（工作目录 D:\luyuan\xuanming-server-mmo-cpp\robot）
1. gofmt -l sync_smoke_scenario.go sync_smoke_scenario_test.go   （期望无输出）
2. go build -mod=vendor ./...
3. go test -mod=vendor -run 'TestSyncSmoke' ./...
   通过标准：TestSyncSmokeJudgeProbe（含 9 个 reject 子用例）、TestSyncSmokePrecleanVerdict、TestSyncSmokeIsReplayedRequest 及原有 TestSyncSmoke* 全过。若编译错误不在 sync_smoke_scenario*.go（工作区里 guild_smoke、ordering_smoke、http_* 等有别的会话在途改动），保留原文另行报告，不要修。

联机：dev-start 后 robot.exe -c etc/sync_smoke.yaml 期望 SYNC_SMOKE_OK；双 zone 用配置副本 cross_zone=true 再跑一次（不要改受跟踪的 yaml）。

## W1：a + e（login / player_locator / gate）

### 修复后最新版

【W1 验证说明（第 2 版，替换实现时那份）】仓库根 D:\luyuan\xuanming-server-mmo-cpp；Go 命令一律先 $env:GOTOOLCHAIN='local'；失败不自动重试，保留首个失败的完整输出；需要 python 3。

〇、前置（同设计 §8，任一项不满足就停下报告）
1. 以下三项由主线确认：fence 批次的验证状态；proto/ 下别的会话的在途改动「可生成」；Kafka 入口生成器重构已完成，且它自己的生成器测试是绿的。
2. 生成（W1、W2 共用一次）：
   pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
   pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml
   W1 期望的变化只有三类：gate_event（KickPlayerEvent 的 2、3 号字段）和 player_locator（SetDisconnectingRequest 4、MarkOfflineRequest 4）的各语言产物；cpp/nodes/gate/handler/event/gate_kafka_command_router.cpp 只在信封分支的 Kick 多出一行 event.set_player_id(command.player_id());；message_id.txt 与事件号无变化，守护段原样保留。
   注意：§8 要求「改 proto 之前」取生成基线，而 proto 已经改在工作区里。主线没有留下改前基线时，这一步只能人工核对上面的清单，并把「生成基线比对未执行」列为未验证项。不要为了取基线去改写共享的 proto 文件。

一、第一轮：复现红（旧行为）
W1 的修复与接缝已一次落完。旧行为用下面的脚本机械切出，跑完逐字节恢复，全程不手工编辑。脚本只动 3 个文件：entergamelogic.go（踢线回到 SetSession 之前，去掉复读消歧和提交后的踢线）、setdisconnectinglogic.go、markofflinelogic.go（去掉实例号判定块）。接缝、新参数和透传全部保留；session_manager.go 不动。
1. 把文末脚本原样写到仓库外，路径 $env:TEMP\w1-redrun\w1_redrun.py。用 PowerShell 单引号 here-string 写入，结束符 '@ 必须顶格；脚本是纯 ASCII，缩进是空格，制表符写成 \t 转义。
2. python $env:TEMP\w1-redrun\w1_redrun.py check D:\luyuan\xuanming-server-mmo-cpp
   必须打印 CHECK OK。交付时三个文件的 orig sha256 如下：
   entergamelogic.go = 4a2448c757ab9adef43792616c4654c3702497f13fba2470c588ce780daffbff
   setdisconnectinglogic.go = 988bb2b20c50fb878a37c83a8fb832aae6969f0bfdbcd6d9488cea011fa668b3
   markofflinelogic.go = d2995dd9b0131700cd1cf00b55ea59257026415fe11ecdc8758e61bc8df47274
   - sha 不同但 CHECK OK：照常执行，在报告里注明。
   - 出现 ABORT：跳过红跑，走第四节。
3. python $env:TEMP\w1-redrun\w1_redrun.py red D:\luyuan\xuanming-server-mmo-cpp $env:TEMP\w1-redrun\backup，应打印 RED OK。
4. 紧接着运行下面的测试，窗口尽量短：
   cd go/player_locator; go test ./internal/logic/ -count=1 -v -run '^(TestSetDisconnecting_StaleGateInstanceIgnored|TestSetDisconnecting_EmptyGateInstanceProceeds|TestSetDisconnecting_SameGateInstanceProceeds|TestMarkOffline_StaleGateInstanceIgnored|TestMarkOffline_EmptyExpectedInstanceProceeds|TestIsStaleGateInstanceReport)$' *> $env:TEMP\w1-red-locator.txt
   cd ../login; go test ./internal/logic/clientplayerlogin/ -count=1 -v -run '^(TestEnterGame_|TestSetSessionProbeConfirmsCommit|TestDisconnect_|TestLeaveGame_|TestCompensateFenceBind_|TestBindSessionFence_)' *> $env:TEMP\w1-red-login.txt
   go test ./internal/logic/pkg/sessionmanager/ -count=1 -v *>> $env:TEMP\w1-red-login.txt
   出现编译错误：先执行第 5 步恢复，再停下报告。
5. 立刻恢复：python $env:TEMP\w1-redrun\w1_redrun.py restore D:\luyuan\xuanming-server-mmo-cpp $env:TEMP\w1-redrun\backup，应打印 RESTORE OK。
   - 出现 ABORT（drift）：说明窗口内别的会话写过这些文件。不覆盖、不手修，保留 backup 目录和 manifest.json，附上 git diff HEAD -- 这 3 个文件的输出，停下等决策。
   - RESTORE OK 之后删除 $env:TEMP\w1-redrun。两份 w1-red-*.txt 保留，作为 AGENTS §11.4 的红跑证据。
6. 判据：结果必须恰好如下，否则报告差异，不算通过。
   - player_locator：TestSetDisconnecting_StaleGateInstanceIgnored、TestMarkOffline_StaleGateInstanceIgnored 为红；其余 4 个为绿。
   - login / clientplayerlogin，踢线行为红（4 个）：
     - TestEnterGame_ReplaceLoginSetSessionFailureDoesNotKick：kicks 非空
     - TestEnterGame_ReplaceLoginKicksOnlyAfterSetSessionCommits：事件序列为 [GetSession Kick SetSession]
     - TestEnterGame_ChainWithoutGateInstanceDoesNotKick/different_session_id：kicks 非空
     - TestEnterGame_CanceledChainDoesNotKick：kicks 非空
   - login / clientplayerlogin，只差计数的红（3 个）。首个失败必须是计数行，这证明踢线次数没有回归：
     - TestEnterGame_SameSessionSameInstanceDoesNotKick：login_kick_total{result="skip_same_session"} delta = 0, want 1
     - TestEnterGame_SameSessionDifferentInstanceSkipsCrossInstance：skip_cross_instance delta = 0, want 1
     - TestEnterGame_ChainWithoutGateInstanceDoesNotKick/same_session_id：skip_no_target delta = 0, want 1
   - login / clientplayerlogin，只记录不作判据（设计标「—」）：
     - UnknownOutcomeProbeConfirmsCommitKicks：预计红，sent 计数为 0
     - ProbeConfirmedCommitPlacesDisconnectLease：预计红，SetDisconnecting 0 次
     - UnconfirmedOutcomePlacesNoLease：预计绿
     - ProbeUsesIndependentDeadline：预计红，GetSession 1 次
     - KickSendFailureDoesNotFailChain：预计红，send_failed 计数为 0
   - login / clientplayerlogin，绿：TestSetSessionProbeConfirmsCommit；其余 TestEnterGame_*；全部 TestDisconnect_* / TestLeaveGame_* / TestCompensateFenceBind_* / TestBindSessionFence_*（含 TestLeaveGame_CrossInstanceReportLeftToLocator）。
   - login / sessionmanager：全绿，含 TestDeleteSession_LeavesGateInstanceCheckToLocator。

二、第二轮：全绿（第一轮 RESTORE OK 之后的工作区）
1. 生成器：cd tools/proto_generator/protogen; go test ./internal/generator/cpp/ -run 'TestGateKafkaRouterAutoMapsSameNameSameTypeFields|TestGateKafkaRouterSkipsSameNameDifferentTypeField' -count=1 -v
   然后 go test ./internal/generator/cpp/... -count=1。若因不属于本批的在途用例变红，停下报告归属，不计为 W1 失败。
2. cd go/player_locator; go test ./internal/... -count=1; go vet ./...
   同包里 W2 的用例（leave_scene_identity_test.go、reconnect_replay_generation_test.go）一并运行，它们的红绿判据归 W2。
3. cd go/login; go test ./internal/svc/... ./internal/logic/... -count=1; go vet ./...
4. C++，串行执行：
   msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
   msbuild cpp\tests\routing_identity_test\routing_identity_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
5. build\cpp\tests\routing_identity_test.exe --gtest_filter=KickFence.*:GateIdentityFence.*:LoginGeneration.*，然后全量运行。
通过标准：原有用例数不减少，全部为绿；第一轮列出的红用例此时全绿，包括 locator 两个计数断言（stale_gate_instance_total 的 op=set_disconnecting 和 op=mark_offline 各加 1）。失败时保留首个失败的完整输出；MSBuild 保留首个错误前后各 50 行。

三、可选联机：同设计 §8 第 5 步。

四、红跑未执行时的替代口径（check 或 red 报 ABORT，或主线决定跳过）
在报告里把「W1 旧行为红跑未执行」列为未验证项，用下面的静态推演作替代证据：
- login：HEAD 的 persistEnterGameSession 先调 kickReplacedSession（existing.SessionID 与本会话号不同、且 GateID 非空就踢，错误只记日志），后调 SetSession。所以第一节第 6 步里「踢线行为红」的 4 个用例在旧代码上必然踢 1 次，或者顺序反过来；旧代码没有 login_kick_total，「只差计数」的 3 个用例必然因计数为 0 而红。
- player_locator：旧 SetDisconnecting 只比会话号，旧 MarkOffline 只比会话号和版本，所以两条 Stale…Ignored 用例必然分别进入 DISCONNECTING、删除会话。

【w1_redrun.py（原样写入）】
# W1 round-1 red-run helper (scene-enter-chain-residuals.md section 8). ASCII only; tabs written as \t escapes.
# Usage: python w1_redrun.py check|red|restore <repo_root> [backup_dir]
#   check   : dry run, writes nothing; prints the red-state diff and sha256 of each file.
#   red     : back up the 3 files byte-for-byte into backup_dir, then mechanically strip the W1 fixes
#             (seams, new params and pass-through stay); manifest.json records sha256 of original and red bytes.
#   restore : refuse unless every file still hashes to its red sha256 (nobody else wrote during the window),
#             then write the backup bytes back and verify they hash to the original sha256.
import difflib, hashlib, json, os, sys

T = "\t"
EG = "go/login/internal/logic/clientplayerlogin/entergamelogic.go"
SD = "go/player_locator/internal/logic/setdisconnectinglogic.go"
MO = "go/player_locator/internal/logic/markofflinelogic.go"

EDITS = {
    EG: [
        (T + "case sessionmanager.FirstLogin, sessionmanager.ReplaceLogin:\n"
         + T*2 + "newSession := sessionmanager.NewOnlineSession(existing, sessionmanager.OnlineSessionInput{\n",
         T + "case sessionmanager.FirstLogin, sessionmanager.ReplaceLogin:\n"
         + T*2 + "if decision == sessionmanager.ReplaceLogin {\n"
         + T*3 + "// W1 red-run only: HEAD's kick-before-commit through the kicker seam (same checks as HEAD kickReplacedSession).\n"
         + T*3 + "if existing != nil && existing.SessionID != state.sessionID && existing.GateID != \"\" {\n"
         + T*4 + "if err := l.replacedKicker().KickSessionOnGate(existing.GateID, existing.GateInstanceID, existing.SessionID, existing.PlayerID, loginGeneration); err != nil {\n"
         + T*5 + "logx.Errorf(\"Failed to kick old session for player %d: %v\", state.playerID, err)\n"
         + T*4 + "}\n"
         + T*3 + "}\n"
         + T*2 + "}\n"
         + "\n"
         + T*2 + "newSession := sessionmanager.NewOnlineSession(existing, sessionmanager.OnlineSessionInput{\n"),
        (T*3 + "if decision == sessionmanager.ReplaceLogin {\n"
         + T*4 + "l.settleReplaceLoginAfterSetSessionError(ctx, existing, state, loginGeneration, err)\n"
         + T*3 + "}\n",
         ""),
        (T*2 + "if decision == sessionmanager.ReplaceLogin {\n"
         + T*3 + "l.kickReplacedSessionAfterCommit(existing, state, loginGeneration)\n"
         + T*2 + "}\n",
         ""),
    ],
    SD: [
        (T + "if isStaleGateInstanceReport(session.GateInstanceId, in.GateInstanceId) {\n"
         + T*2 + "staleGateInstanceTotal.Inc(staleGateInstanceOpSetDisconnecting)\n"
         + T*2 + "l.Infof(\"SetDisconnecting: stale gate instance player=%d session=%d record_instance=%s request_instance=%s, ignoring\",\n"
         + T*3 + "in.PlayerId, in.SessionId, session.GateInstanceId, in.GateInstanceId)\n"
         + T*2 + "return &common.Empty{}, nil\n"
         + T + "}\n",
         ""),
    ],
    MO: [
        (T + "if isStaleGateInstanceReport(current.GateInstanceId, in.ExpectedGateInstanceId) {\n"
         + T*2 + "staleGateInstanceTotal.Inc(staleGateInstanceOpMarkOffline)\n"
         + T*2 + "l.Infof(\"MarkOffline: stale gate instance ignored player=%d session=%d/%d record_instance=%s request_instance=%s\",\n"
         + T*3 + "playerID, current.SessionId, current.SessionVersion, current.GateInstanceId, in.ExpectedGateInstanceId)\n"
         + T*2 + "return &common.Empty{}, nil\n"
         + T + "}\n",
         ""),
    ],
}


def read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


def write_bytes(path, data):
    with open(path, "wb") as f:
        f.write(data)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def strip_fixes(path, data):
    text = data.decode("utf-8")
    for old, new in EDITS[path]:
        n = text.count(old)
        if n != 1:
            sys.exit("ABORT %s: expected exactly 1 match of a fix block, found %d (file changed since hand-off)" % (path, n))
        text = text.replace(old, new)
    return text.encode("utf-8")


def main():
    mode, root = sys.argv[1], sys.argv[2]
    if mode == "check":
        for path in EDITS:
            data = read_bytes(os.path.join(root, path))
            red = strip_fixes(path, data)
            diff = difflib.unified_diff(data.decode("utf-8").splitlines(True), red.decode("utf-8").splitlines(True),
                                        "fixed/" + path, "red/" + path)
            sys.stdout.buffer.write("".join(diff).encode("utf-8"))
            sys.stdout.buffer.flush()
            print("%s orig=%s red=%s" % (path, sha(data), sha(red)))
        print("CHECK OK")
        return
    backup = sys.argv[3]
    manifest_path = os.path.join(backup, "manifest.json")
    if mode == "red":
        if os.path.exists(manifest_path):
            sys.exit("ABORT: %s exists; the previous red run was not restored" % manifest_path)
        os.makedirs(backup, exist_ok=True)
        staged = {}
        for path in EDITS:
            data = read_bytes(os.path.join(root, path))
            staged[path] = (data, strip_fixes(path, data))
        manifest = {}
        for i, (path, (data, red)) in enumerate(staged.items()):
            name = "%d.orig" % i
            write_bytes(os.path.join(backup, name), data)
            manifest[path] = {"backup": name, "orig": sha(data), "red": sha(red)}
        write_bytes(manifest_path, json.dumps(manifest, indent=2).encode("ascii"))
        for path, (data, red) in staged.items():
            write_bytes(os.path.join(root, path), red)
        print(json.dumps(manifest, indent=2))
        print("RED OK")
        return
    if mode == "restore":
        manifest = json.loads(read_bytes(manifest_path).decode("ascii"))
        drift = [p for p, m in manifest.items() if sha(read_bytes(os.path.join(root, p))) != m["red"]]
        if drift:
            sys.exit("ABORT: changed by someone else during the red run, NOT restored: %s" % drift)
        for path, m in manifest.items():
            data = read_bytes(os.path.join(backup, m["backup"]))
            if sha(data) != m["orig"]:
                sys.exit("ABORT: backup of %s is corrupt, NOT restored" % path)
        for path, m in manifest.items():
            write_bytes(os.path.join(root, path), read_bytes(os.path.join(backup, m["backup"])))
            if sha(read_bytes(os.path.join(root, path))) != m["orig"]:
                sys.exit("ABORT: %s does not hash back to the original after restore" % path)
        os.remove(manifest_path)
        print("RESTORE OK")
        return
    sys.exit("unknown mode " + mode)


main()

## W2：b / c / d 的 Go 侧（scene_manager / player_locator）

### 修复后最新版

本节命令都在 PowerShell 7 里跑，根目录 $root = 'D:\luyuan\xuanming-server-mmo-cpp'。

每步红了就停，保留首个失败的完整输出，不要连续自动重试。第一轮里下表标「红」的预期失败不触发停止；出现预期之外的失败或编译错误才停。

====================
第一轮：复现（红跑，W2 的 Go 部分）
====================

前置条件：先按设计稿 §8 完成「前置门禁」「生成基线」和第一轮第 2 步。也就是在主工作区跑 proto-gen-build，再用 no-unity 配置跑 proto-gen-run，然后与基线逐文件比对，只允许出现 §8 清单内的变化。W2 的 proto 改动已在工作区，不要再改 proto。

修复代码已经在主工作区里了，所以红跑放在仓库外的临时副本里做，主工作区一个字节都不动。

1. 建临时副本，把 W2 的纯修复文件换回 HEAD（git archive 逐字节导出，避免管道重编码中文）：

```
$root = 'D:\luyuan\xuanming-server-mmo-cpp'
$red  = Join-Path $env:TEMP ('w2-red-' + (Get-Date -Format yyyyMMddHHmmss))
New-Item -ItemType Directory -Force $red | Out-Null
robocopy "$root\go" "$red\go" /E /NFL /NDL /NJH /NJS
if ($LASTEXITCODE -ge 8) { throw 'robocopy failed' }

git -C $root archive --format=tar -o "$red\w2-head.tar" HEAD `
  go/scene_manager/internal/logic/enterscenelogic.go `
  go/scene_manager/internal/logic/changesceneutil.go `
  go/scene_manager/internal/logic/owner_epoch.go `
  go/scene_manager/internal/logic/leavescenelogic.go `
  go/player_locator/internal/logic/leasemonitor.go `
  go/player_locator/internal/logic/reconnectlogic.go
tar -xf "$red\w2-head.tar" -C $red
```

2. 只改 HEAD 版 enterscenelogic.go 第 5b 步（HEAD :725-726）这一处接缝，位置不动：

```
$f = "$red\go\scene_manager\internal\logic\enterscenelogic.go"
$t = [IO.File]::ReadAllText($f)
$n = $t -replace 'dispatchReleasePlayer\(l\.svcCtx, l\.Logger, currentZoneID,(\r?\n\s*)currentLoc\.NodeId, in\.PlayerId, sceneId, nodeId\)', 'releaseDispatcher(l.svcCtx, l.Logger, currentZoneID,$1currentLoc.NodeId, in.PlayerId, sceneId, nodeId, observedEpoch)'
if ($n -eq $t) { throw '5b replace missed' }
[IO.File]::WriteAllText($f, $n, [Text.UTF8Encoding]::new($false))
```

红跑树里保留工作区版本的文件（接缝 / 纯声明 / 测试）：
- placement_identity.go
- scene_node_client.go
- constants/errors.go
- metrics/metrics.go
- integration_test.go
- 全部 W2 新测试：enterscene_identity_test.go、leavescene_identity_test.go、placement_identity_test.go、placement_lua_expect_test.go、release_dispatch_test.go，以及 player_locator 下的 scene_manager_client_fake_test.go、leave_scene_identity_test.go、reconnect_replay_generation_test.go
- 已生成的 go/proto

【与 W1 协调】player_locator 包里也有 W1 的在途修复文件（markofflinelogic.go、setdisconnectinglogic.go 等）。在同一个 $red 里按 W1 自己的红跑配方处理它们，再跑 player_locator。W1 用例的红绿不计入 W2 的判据。

3. 运行：

```
$env:GOTOOLCHAIN = 'local'
Set-Location "$red\go\scene_manager"
go test ./internal/... -count=1 -v 2>&1 | Tee-Object "$red\sm-red.txt"
go vet ./...
Set-Location "$red\go\player_locator"
go test ./internal/... -count=1 -v 2>&1 | Tee-Object "$red\pl-red.txt"
go vet ./...
```

第一轮通过标准：

(a) 两个模块都能编译，不能出现 undefined、build failed。

(b) 顶层「--- FAIL」中 W2 的部分恰好是下面 15 条（标「红」）：

scene_manager 14 条：
- TestLeaveScene_StaleLeaveAfterSamePlacementRelogKeepsLocation
- TestLeaveScene_SameConnectionRelogKeepsLocation
- TestLeaveScene_NewerPlacementResolvedByLocatorKept
- TestLeaveScene_SameSceneRewrittenByNewerLoginKept
- TestEnterScene_RouteFailureDoesNotDispatchRelease
- TestEnterScene_PlacementFailureDoesNotDispatchRelease
- TestEnterScene_ReleaseCarriesObservedOwnerEpoch
- TestEnterScene_DevBypassReleaseCarriesUnmintedEpoch
- TestEnterScene_StaleLoginAfterNewerSamePlacementRejected
- TestEnterScene_StaleSessionSceneChangeRejected
- TestEnterScene_PlacementRejectedWhenIdentityChangedSinceObservation
- TestEnterScene_FirstPlacementRejectedWhenLocationAppearedSinceObservation
- TestEnterScene_CrossZoneFirstLegRejectedWhenLocationChangedSinceObservation
- TestEnterScene_CrossZoneFirstLegStaleRejected

player_locator 1 条：
- TestReconnect_ReplayWithDifferentGenerationRejected

(c) 标「—」的只记录结果、不作判据（红跑树里多数会失败，属预期）：
- TestEnterScene_SameSessionWithoutGenerationAdmittedPreservesGeneration
- TestEnterScene_UnversionedDifferentSessionAdmittedAsLegacy
- TestEnterScene_LegacyAdmitDoesNotDowngradeIdentity
- TestEnterScene_SamePlacement*（3 条）
- TestEnterScene_NoGateIdCrossNodeDispatchesAfterPlacement
- TestDispatchReleasePlayer_RetriesCarrySameExpectedEpoch
- TestLeaveScene_EqualGeneration*（2 条）
- TestMintLua* / TestSetLocationLua*
- player_locator 的 TestMarkOffline_LeaveSceneCarriesDeletedSessionIdentity / TestLeaseExpiry_LeaveSceneCarriesSessionIdentity / TestMarkOffline_EnqueuedRetryPreservesGeneration

(d) 下列必须绿：
- 原有全部用例，含 TestEnterScene_RouteEventCarriesRequestLoginGenerationAcrossSamePlacementTakeover，以及 logic_test.go / owner_epoch_test.go 里「不得调用 ReleasePlayer」的断言
- TestLeaveScene_OlderLocationIdentityIsDeleted
- TestLeaveScene_LegacyRequestWithoutIdentityDeletes / TestLeaveScene_LegacyLocationWithoutIdentityDeletes
- TestReconnect_ReplayWithSameGenerationSucceeds / TestReconnect_LegacyReplayWithZeroGenerationSucceeds
- placement_identity_test.go 全部（纯函数文件在树里）

sm-red.txt / pl-red.txt 留作 AGENTS §11.4 的红跑证据。保存好之后删除 $red\go 副本。

C++ 部分（W3 的 cross_zone_test）和 login 部分（W1）的红跑按 §8 第 3、4 步及各组配方执行，不在本组配方内。

====================
第二轮：修复（在主工作区原地跑，不再改 proto、不重新生成）
====================

```
$env:GOTOOLCHAIN = 'local'
Set-Location 'D:\luyuan\xuanming-server-mmo-cpp\go\scene_manager'
gofmt -l internal/logic/placement_identity.go internal/logic/enterscenelogic.go internal/logic/changesceneutil.go internal/logic/owner_epoch.go internal/logic/leavescenelogic.go internal/logic/scene_node_client.go internal/logic/enterscene_identity_test.go internal/logic/leavescene_identity_test.go internal/logic/placement_identity_test.go internal/logic/placement_lua_expect_test.go internal/logic/release_dispatch_test.go internal/logic/integration_test.go internal/constants/errors.go internal/metrics/metrics.go
go test ./internal/logic/ -count=1 -v -run 'TestEnterScene_(StaleLogin|StaleSession|SameSession|Unversioned|LegacyAdmit|SamePlacement|PlacementRejected|FirstPlacementRejected|CrossZoneFirstLeg|RouteFailureDoesNot|PlacementFailureDoesNot|ReleaseCarries|DevBypassRelease|NoGateIdCrossNode|RouteEventCarriesRequestLoginGeneration)|TestLeaveScene_|TestDispatchReleasePlayer_|TestDecidePlacementIdentity|TestDecideLeaveIdentity|TestPlacementVerdictNames|TestLeaveVerdictNames|TestValidSession|TestErrPlacementSessionSuperseded|TestMintLua|TestSetLocationLua'
go test ./internal/... -count=1
go vet ./...

Set-Location 'D:\luyuan\xuanming-server-mmo-cpp\go\player_locator'
gofmt -l internal/logic/leasemonitor.go internal/logic/reconnectlogic.go internal/logic/scene_manager_client_fake_test.go internal/logic/leave_scene_identity_test.go internal/logic/reconnect_replay_generation_test.go
go test ./internal/logic/ -count=1 -v -run 'TestMarkOffline_LeaveSceneCarriesDeletedSessionIdentity|TestLeaseExpiry_LeaveSceneCarriesSessionIdentity|TestMarkOffline_EnqueuedRetryPreservesGeneration|TestReconnect_'
go test ./internal/... -count=1
go vet ./...
```

接着按设计稿 §8「验证命令」做其余各步：
- 第 1 步：生成器测试 TestGateKafkaRouterAutoMaps 及 ./internal/generator/cpp/...
- 第 2 步：go/login
- 第 3、4 步：C++ 部分。msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64，必须串行；然后跑 cross_zone_test.exe --gtest_filter=ReleaseFence*:ActingLoginGeneration*:EnterFence*:*Travel*:*Relocate*，再跑全量。

第二轮通过标准：
- gofmt 无输出
- 定向与全量用例全部绿
- 原有用例数不减少
- go vet 退出码 0

失败时保留：首个失败用例的完整输出、生成产物与基线的比对结果、MSBuild 首个错误前后 50 行。

====================
提交前（不是 Codex 执行项，提醒主线）
====================

W2 提交要按 hunk 排除下列内容，它们属于别的会话：
- enterscenelogic.go 的「@@ -600,7 +625,9 @@」（G4a 注释）
- scene_node_client.go 的「@@ -117,7 +117,7 @@」（CreateScene 注释）
- metrics.go 的 load_refresh 部分
- scene_node_service.proto 的 AssetAckReceipt 两行

## W3：b / c / d 的 C++ scene 侧

### 修复后最新版

本轮只改注释，不需要单独验证，跟设计 §8 的两轮一起跑即可。前置条件：W1/W2 改完 proto，并已在仓库根执行 pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build 和 pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml，生成后 cpp/generated 里能 grep 到 expected_owner_epoch。
1）在仓库根串行编译：msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64。然后串行构建 cpp/tests/cross_zone_test 测试工程（/m:1）。失败时保留 MSBuild 首个错误前后 50 行。
2）运行 cross_zone_test.exe --gtest_filter=ReleaseFence*:ActingLoginGeneration*:EnterFence*:*Travel*:*Relocate*，再跑全量 cross_zone_test.exe。
3）判定标准：第一轮（不含修复）设计 §4.7/§5.7 表中标「红」的用例恰好失败，其余为绿；第二轮全部为绿，原有用例数不减少。本轮改的 release_fence.h 和 player_lifecycle.cpp 都只动注释，预期对编译和用例结果没有影响。失败时保留首个失败用例的完整输出，不连续自动重试。

### 实现时的完整版（含第一轮临时补丁）

工作目录：仓库根 D:\luyuan\xuanming-server-mmo-cpp，在 x64 的 Developer PowerShell for VS 中执行。MSBuild 一律串行 /m:1。
严禁用 git checkout -- / git stash / git reset 还原：相关文件里有别的会话的在途改动。

【前置】W1/W2 的 proto 已改完。按设计 §8 先建生成基线，再生成：
  pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-build
  pwsh -File tools/scripts/dev_tools.ps1 -Command proto-gen-run -UseBinary -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml
确认生成的 scene_node_service.pb.h 里有 expected_owner_epoch()：
  rg -n "expected_owner_epoch" cpp/generated
纯度自检，应无输出：
  rg -n "entt|redis|protobuf|google::" cpp/libs/services/scene/player/system/release_fence.h

【第一轮：红跑。临时补丁，跑完手工还原】
补丁 1：cpp/libs/services/scene/player/system/player_lifecycle.cpp 的 HandleReleaseFromSceneManager
  (a) switch 里 case kSkipStaleRelease / kSkipAheadOfHolder 打完 LOG_WARN 后的 `return;` 改为 `break;`
  (b) `if (!release_fence::Releases(verdict))` 改为 `if (verdict == release_fence::Verdict::kCount)`
  效果：照常判定、计数，但只有「交接在途」会挡住释放，等同设计 §8 所说的「守护段搬进库函数、暂不加栅栏」。
补丁 2：cpp/libs/services/scene/player/system/player_lifecycle.h 的 SmRejectedBeforeAnyPlacementWrite，删掉 `case kSmErrPlacementSessionSuperseded:` 这一行。
构建：
  msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
  msbuild cpp\tests\cross_zone_test\cross_zone_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
运行（exe 位于 cross_zone_test.vcxproj 的 OutDir）：
  cross_zone_test.exe --gtest_filter=ReleaseFence*:ActingLoginGeneration*:RelocateConfirmKick.*
期望恰好以下 5 条失败：
  ReleaseFenceEcs.LateReleaseAfterRollbackAdoptKeepsEntity
  ReleaseFenceEcs.LateReleaseOnReturnedEntityIgnored
  ReleaseFenceEcs.AheadOfHolderIgnored
  ReleaseFenceEcs.StaleReleaseOnExitingEntityDoesNotMerge
  RelocateConfirmKick.PlacementSessionSupersededIsRejectedBeforeAnyPlacementWrite
其余（ReleaseFence.*、EqualEpochReleases、UnversionedReleaseKeepsLegacy、CacheUnknownReleasesAndCounts、HandoffInFlightStillIgnored、PlayerNotOnThisNodeIsIdempotent、ActingLoginGeneration.*、原有 RelocateConfirmKick.*）全绿。不应出现崩溃。保存完整输出作为 AGENTS §11.4 的红跑证据。
还原：手工把上面三处改回原样，再用 git diff 核对这两个文件只剩本组的改动。

【第二轮：绿跑】
  msbuild game.sln /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
  msbuild cpp\tests\cross_zone_test\cross_zone_test.vcxproj /m:1 /nr:false /p:Configuration=Debug /p:Platform=x64
  cross_zone_test.exe --gtest_filter=ReleaseFence*:ActingLoginGeneration*:EnterFence*:*Travel*:*Relocate*
  cross_zone_test.exe            （全量）
通过标准：新增 16 条用例全绿；原有用例数不减少，全部为绿。

【失败时保留】
- 首个失败用例的完整输出；
- MSBuild 首个错误前后 50 行。若是 scene_node_service.cpp 报 expected_owner_epoch 不存在，说明 W2 的 proto 或生成没到位，不算本组失败；
- 生成产物与基线的比对结果。

不要连续自动重试。

【可选联机】一区、路由模式。观察 scene 日志里的 [ReleaseFence] 行：成功交接后应以 skip_handoff_in_flight / not_found 为主；SM 全量升级后 release_unversioned 不再增长。

## 围栏 v0.4：C++ 进程级发号器（2026-10-16 补）

> 主线补改：`SaveSeqFloorFrom` 已从 `save_seq.h` 拆到 `player/system/save_seq_floor.h`（`save_seq.h` 经 `core/system/id_generator.h` 被大量编译单元引入，不应带进 `player_cache.pb.h`）；第 5 步静态核对另加 `git grep -n "player_cache.pb.h" -- cpp/libs/services/scene/player/system/save_seq.h`，预期无输出。

工作目录：D:\luyuan\xuanming-server-mmo-cpp（仓库根），在 VS 开发者 PowerShell 环境下执行。前置：不需要清 Redis/MySQL（纯单测）；别的会话的在途改动保持原样，不要 reset。

1. 全量构建（必须串行 /m:1；scene 库、id_generator.h、所有包含它的测试工程都会重编）：
msbuild game.sln /m:1 /p:Configuration=Debug /p:Platform=x64 /fl "/flp:logfile=build\saveseq-msbuild.log;verbosity=normal"
失败时只保留第一个错误前后 50 行：
Select-String -Path build\saveseq-msbuild.log -Pattern ': error ' -Context 50,50 | Select-Object -First 1
预期产物：build\cpp\tests\cross_zone_test.exe、bag_test.exe、currency_test.exe，以及 aoi_test、aoi_delivery_test、skill_test（它们包含 id_generator.h）。重点关注的编译单元：save_seq.h（被 id_generator.h 引入，会带进 player_cache.pb.h）、player_lifecycle.cpp、redis.cpp、save_seq_test.cpp。

2. 全量运行（脚本会先同步 zlibd/rdkafka 的 DLL）：
pwsh tools/scripts/run_cpp_tests.ps1 -Filter cross_zone_test
pwsh tools/scripts/run_cpp_tests.ps1 -Filter bag_test
pwsh tools/scripts/run_cpp_tests.ps1 -Filter currency_test
pwsh tools/scripts/run_cpp_tests.ps1 -Filter aoi
pwsh tools/scripts/run_cpp_tests.ps1 -Filter skill_test
（Replace 改动会直接编进 bag_test/currency_test；id_generator.h 改动影响 aoi/aoi_delivery/skill，这几个工程至少要编译通过并跑一遍。）

3. 过滤运行新用例：
build\cpp\tests\cross_zone_test.exe --gtest_filter=SaveSeq.*:SaveSeqEcs.*
预期 14 个用例全过：SaveSeq.NextMonotonic、NextHonorsWallClockFloor、NextMonotonicUnderClockRollback、NextSameMillisecond、ObserveRaisesFloor、ObserveRejectsBeyondBound、NextSaturatesWithoutWrap、PersonalFloorBeyondBound、PersonalFloorSaturates、FloorFromAllData、PersistedPayloadCurrentIgnoresSeq、SnapshotStripsLastSaveSeq；SaveSeqEcs.SecondUnchangedSaveHitsFastPath、SaveSeqEcs.FastPathIgnoresDifferingLastSaveSeq。

4. 通过标准：每个工程 PASSED 数等于 Running 数，FAILED 为 0；cross_zone_test 的用例总数等于本批之前的数（设计记的是 166，以本次之前实跑的数为准）加 14；bag_test、currency_test 的用例数不减。失败时保留该用例的完整 gtest 输出，以及首个失败断言前后 50 行。

5. 代码评审项的静态核对（只读）：
git grep -n "lastSaveSeq" -- cpp            （预期无输出）
git grep -n "set_fence_" -- cpp/libs        （预期无输出：C++ 不写 body 围栏）
git grep -n "dirty_save::IsEqual(\*message, \*snap->snapshot)" -- cpp/libs/services/scene/player/system/player_lifecycle.cpp   （预期 1 处：快路径已改回）

## 围栏 v0.4：go/db `Fencing.Mode` 开关（2026-10-16 补）

工作目录 `go/db`，`$env:GOTOOLCHAIN='local'`：

1. `go test ./internal/config/ -run "TestConfigValidateRejects|TestEtcYamlRetryKeysValid" -count=1 -v`：新增 5 行 Fencing 用例（缺省 / legacy / dual 通过，`Dual`、`sql` 拒绝）。
2. `go test ./internal/kafka/ -run "TestSavePlayerRowLegacyModeLeavesPlayerTablesToPlainSave|TestSetFencingModeRejectsUnknownMode" -count=1 -v`。
3. 真库（自起一次性 MySQL，不借 codex-* 容器；`go test` 报 Access denied 时改用 `go test -c` 编出二进制再执行）：`ASSET_FENCE_TEST_DSN=<不带库名的 DSN>` 下跑 `TestIntegrationPlayerSQLFence*`，夹具已切 dual 并在 Cleanup 复位 legacy。
4. `go test ./... -count=1` 与 `go vet ./...` 全量回归（含 go/db 会话 R1–R3 的改动，失败先看归属）。
5. 启动核对：`db.exe` 启动日志有 `player row fencing: mode=legacy`。

## 围栏 v0.4：资产通道 SQL 证据开关与自动放弃（2026-10-16 补）

> **前置耦合（主线补注）**：mail 的 Finalize、trade 的合同 / 商城到账（ORDER）强制要求 SQL 证据；证据只在 go/db `Fencing.Mode=dual` 时发布。所以这些在途功能要在其投递到的 zone 的 go/db 切 dual 之后才能终结——联机验证 mail / 商城到账前先把 go/db 切 dual，否则会一直停在 AwaitDurable。

工作目录：仓库根 D:\luyuan\xuanming-server-mmo-cpp。下面用 bash 写法；PowerShell 下先执行 $env:GOTOOLCHAIN='local'，再去掉每条前面的 GOTOOLCHAIN=local 前缀。各模块彼此独立，可以按顺序串行跑。失败时保留 go 输出里失败用例的名字和断言文本。

0) 格式检查，期望无输出：
gofmt -l go/shared/assetop go/guild/guild.go go/guild/internal/config go/guild/internal/svc go/trade/internal/config go/trade/internal/reconcile go/trade/internal/svc go/mail/mail.go tools/merge_zone

1) shared/assetop（本次主体）：
cd go/shared && GOTOOLCHAIN=local go vet ./assetop/... && GOTOOLCHAIN=local go test -count=1 ./assetop/...
如需单独看重点用例：GOTOOLCHAIN=local go test -count=1 ./assetop/ -run 'SQLProof|SQLCommit|CheckWiring|OfflineLedgerRead|EscrowReceiptChecked|AbandonWithSQLCommit|AbandonNotRunningWithoutSQLLedger|AbandonOnce|FinalizeKeepsBudget|AwaitSQLKeepsBudget|FinalizeSurvivesCanceledParent|UnconfirmedResult|WakePlayerStopsClaiming|DefaultLoopConfigGolden|DataServiceLedger|ReceiptAck|AbandonSkipNames' -v
通过标准：全绿。发布脚本的用例跑在 miniredis 上（gopher-lua），测不到真 Redis 的 locale。

2) guild：
cd go/guild && GOTOOLCHAIN=local go build ./... && GOTOOLCHAIN=local go vet ./... && GOTOOLCHAIN=local go test -count=1 ./internal/svc/... ./internal/config/... ./internal/logic/... ./internal/data/...
（没设置 GUILD_TEST_MYSQL_DSN 时，真库集成用例会自行 skip。）

3) trade：
cd go/trade && GOTOOLCHAIN=local go build ./... && GOTOOLCHAIN=local go vet ./... && GOTOOLCHAIN=local go test -count=1 ./internal/config/... ./internal/reconcile/... ./internal/svc/... ./internal/data/... ./internal/commerce/...

4) mail（只改了 mail.go）：
cd go/mail && GOTOOLCHAIN=local go build ./... && GOTOOLCHAIN=local go vet ./... && GOTOOLCHAIN=local go test -count=1 ./...

5) go/db（只做编译检查：PublishSQLCommit 签名没变，确认调用方仍能编过；go/db 本线未改）：
cd go/db && GOTOOLCHAIN=local go build ./... && GOTOOLCHAIN=local go vet ./internal/kafka/...

6) data_service（只依赖 SQLCommitKey，编译加相关单测）：
cd go/data_service && GOTOOLCHAIN=local go build ./... && GOTOOLCHAIN=local go test -count=1 ./internal/logic/ -run 'SQLCommit|AssetOpLedger'

7) merge_zone 单测：
cd tools/merge_zone && GOTOOLCHAIN=local go vet ./... && GOTOOLCHAIN=local go test -count=1 ./...
重点用例：-run 'TestPlayerCacheKeys'

8) merge_zone 真库集成（可选）：需要本机 127.0.0.1:3306 MySQL 与 127.0.0.1:6379 Redis，按 integration_test.go 文件头准备。itReset 会清理测试数据，不要对着共享环境跑。
cd tools/merge_zone && GOTOOLCHAIN=local go test -count=1 -tags merge_integration -run TestIT_InvalidatePlayerCaches_DeletesLoginAndDbCacheKeys ./...
