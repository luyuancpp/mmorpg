
8. **压测复盘只读 `stress_summarize.ps1` 输出,不要手 grep raw prom dump**。
   - 命令: `pwsh tools/scripts/stress_summarize.ps1 -RunDir robot/logs/stress-<name>-<ts>`
   - 输出五段二维表:robot 每分钟 stats + entergame_total + dataloader stage avg + SceneManager EnterScene 子阶段 + DB task 子阶段 + Kafka lag,~3KB 上下文。
   - **Round 16+ 推荐用 `tools/scripts/stress_snap.ps1` 后台批量拉 snapshot**(并行 scrape 三端口):
     `pwsh tools/scripts/stress_snap.ps1 -RunDir robot/logs/stress-<name>-<ts> -StartTime '<yyyy-MM-dd HH:mm:ss>' -Stages 2,5,10,15,18`
     端口分工: `:9101` login / `:9150` scene_manager / `:9160` db。
     文件命名 `t<N>m_login.txt` / `t<N>m_sm.txt` / `t<N>m_db.txt` —— summarize 脚本自动按后缀分流。
   - 旧式单端口手拉(legacy 兼容): `curl -s http://127.0.0.1:9101/metrics > $RunDir/prom-snapshots/t<n>m.txt`。
   - 历史复盘文档(`stress-1zone-*.md`)直接复用脚本输出的表格,不要再贴 raw count/sum 数字。
9. **压测前后**强制按以下顺序操作,任何一步漏了都重来:
   1. **跑测前** — 把上一次压测的 `stress_summarize.ps1` 输出存为 `prev-summary.txt`(放在 `docs/design/stress-<round>-<date>.md` 同目录或 commit 到对比 PR 描述里),作为 Round N 的对比基线。`prev-summary.txt` 不存就不许开下一轮。
   2. **跑测前** — 清空所有可能污染数据的日志/缓存:
      - `robot/logs/stress-*` 旧目录(留最近 1 个备查,其余删掉)
      - `bin/log/*` cpp gate/scene 日志
      - 各 go service stderr/stdout(`go-svc-stop` + 删 `tools/scripts/.run/` 的 pid/log)
      - `redis-cli FLUSHALL` 清掉残留 lock / session / task:result key
      - kafka topic offset reset(`pwsh tools/scripts/dev_tools.ps1 -Command kafka-offset-reset`)+ broker 数据目录可选清,生产/历史不要清
      - prom snapshot 目录 = 新建 `robot/logs/stress-<name>-<ts>/prom-snapshots/`
   3. **压测中** — 至少在 ramp 完成 / 稳态中段 / 稳态末 三个时刻拉 snapshot 进 prom-snapshots/。
   4. **跑测后** — 跑 `stress_summarize.ps1` 出 Round N 表,与 `prev-summary.txt` 二维对比写进新复盘文档,贴 ARCH §11 决策行 + 更新 CLAUDE.md §7
   5. **压期间不能上传任何日志**
   6. **每次登录压测的话你帮我把所有redis mysql etcd 数据全部删除再新的压测**
   7. **不要读client目录**
   8. **保证修改代码的正确性，数据一致性**
   9. **中文回复**
   已完成清单。**不要在没有对比表的情况下声明"性能提升"**。
