# Chat Sensitive-Word Filter — Design (todo.md #68)

> **2026-10-15 现行接线：Go `go/chat` 与 Java `game-chat` 已接入原 Go 仓的敏感词语义。** 这是八域移植的聊天首批切片，不代表所有频道、可靠推送或聊天能力已对齐。历史 C++ 原型的 ASCII 忽略大小写规则不作为当前 Go/Java 的执行语义，见本节修订。

## 2026-10-15：两版真实发送链

基线 `xuanming-server@b1f6c1ebf4583b45a9370ad0522c64196dc792b2` 的 `services/social/chat/internal/biz/chat.go` 在发送前按 `SensitiveWords` 配置顺序调用 `strings.ReplaceAll`，空词跳过，一个 Unicode 字符替换为一个 `*`。**实际区分大小写**；源配置注释的“不区分大小写”与可执行代码不符，本批保留实际逻辑，不静默改成 C++ 原型规则。不做正则、Unicode 归一化或热更新。

Go 的 `internal/textfilter` 与 Java 的 `SensitiveWordFilter` 启动时构造不可变快照，真实 `SendChat` 在原始消息 512 字节校验后、序列化和历史写入前屏蔽，推送与历史补拉使用同一份处理后正文。空词表保持原行为；配置最多 1024 条、单词最多 256 UTF-8 字节、合计最多 64 KiB，非法 Unicode/超限在启动配置校验时拒绝。Go 配置 `SensitiveWords`，Java `game.chat.sensitive-words`；运行时不查询词库、不更改幂等、会话、跨区路由与频道权限。

验证：Go `go test ./...`、`go build ./...`、`go vet ./...` 全部通过（`GOTOOLCHAIN=local GOPROXY=off`）。Java 从 HEAD 构造隔离快照并覆盖本批 7 文件，离线运行 `ChatServiceTest,SensitiveWordFilterTest,ChatPropertiesTest`，16 项通过；7 文件 SHA 与快照一致。测试覆盖真实发送入历史、补拉屏蔽结果、原始长度先拒绝、大小写/重叠顺序/Unicode/空表及配置绑定。未跑真实 Kafka/Redis、gate/客户端跨区冒烟或容量测试；没有性能提升结论。快照与日志位于 `build/chat-sensitive-20261015/`。

> **Status:** 2026-05-14 — filter primitive landed
> (`sensitive_word_filter.h`); the chat handler pipeline itself does
> not exist yet, so wire-up is not done. This doc says what's in,
> what's out, and how the rest connects when chat lands.

---

## Problem

When chat opens, the channel will be flooded with gold-selling ads,
phishing links, and profanity within minutes of public exposure. The
content team will deliver a word list (per-deployment, per-locale);
the runtime needs a fast scanner that rejects or redacts hits before
the message goes to the broadcast layer.

todo #68 calls out the gap. The chat system itself is still in proto
stage (`proto/chat/chat.proto` + generated grpc stubs, no `cpp/nodes/chat/`
or `cpp/libs/services/chat/` yet), so this slice can't wire to a
running chain. What it CAN do is land the filter primitive so chat
work has one less thing to build when it starts.

---

## Decision

### Byte-substring match over a flat `unordered_set<string>`

Not Aho-Corasick, not a trie, not regex. For the v1 word-list scale
we expect (a few thousand words, ≤512-char chat messages):

| Algorithm | Build time | Per-message scan | When it pays off |
|---|---|---|---|
| Flat `unordered_set` (this) | O(N), trivial | O(L · K) per message, L = msg len, K = max word len | ≤10k words, ≤512-char messages |
| Aho-Corasick | O(Σ word_len) | O(L + matches) | 50k+ words or long messages (logs) |
| Trie | O(Σ word_len) | O(L · K) but cheaper constants | Memory pressure |

The flat-set scan does ~4k unordered_set lookups for a typical chat
message (L=512, K=8), which is well under 100µs on modern x86. Aho-
Corasick is the right next step when v2 needs it; we don't pre-pay
the complexity for a v1 deployment.

### UTF-8 bytes, not code points

Words and input are both raw byte strings. The dictionary owner enumerates
full-width / half-width / homoglyph variants explicitly. This means:
- **No ICU / boost.locale dependency** — keeps the build slim.
- **Predictable cost** — every byte position is a single substring
  lookup, no Unicode normalization on the hot path.
- **Trade-off:** "near miss" detection is the dictionary's job, not
  the engine's. Acceptable for v1; revisit if we see a lot of
  obfuscation in production logs.

### ASCII-only case folding

Hits English / pinyin profanity cheaply. CJK characters pass through
unchanged because the high bit keeps them outside `A`..`Z`. The case
fold is applied to **both** dictionary words at load time and input
at scan time so the comparison is symmetric.

### `Redact` preserves original bytes outside matches

The scanner walks the lowered copy to detect matches, but emits from
the original input — so user-visible characters keep their original
case outside a hit span. Matters when a downstream system stores the
chat verbatim for moderation appeal.

### Immutable-after-Load, swap-the-pointer to reload

`SensitiveWordFilter` instances are immutable after `Load()`. Concurrent
readers (chat broadcast threads) all hit the same instance with no
locking. Word-list reload constructs a fresh instance and atomically
swaps an owning `shared_ptr` at the singleton site (TBD in slice B).

---

## What landed in this commit (2026-05-14)

| File | Change |
|---|---|
| `cpp/libs/engine/core/text/sensitive_word_filter.h` (new) | Header-only filter: `Load(iterable)` / `Contains(view)` / `Redact(view, mask='*')`. No dependencies beyond `<string>` / `<unordered_set>`. |
| `docs/design/chat-sensitive-word-filter.md` (this file) | Decision record. |

The primitive can be unit-tested in isolation, used by future
moderation tools, or wired into a non-chat path (nickname validation,
mail subject screen, etc.) without building the chat chain first.

---

## What's NOT done (slices to track)

### A. Word-list source + reload — **S, ~1 day**

Pick where the dictionary lives:
- Excel table → `cpp/generated/table/code/sensitive_words_table.cpp`
  (auto reload on table hot-update — matches the project's existing
  config pattern)
- OR Redis-backed list, fetched once at boot + watched for ops-pushed
  updates without a node restart

Recommendation: **start with the Excel table**. Reload-on-table-update
is already wired through `data_table_exporter`; ops gets push-to-prod
via the same flow that handles every other config knob. The Redis path
is a slice C optimization for "deployments that change the list >10x/day".

### B. Singleton + hot-reload swap — **S, ~half day**

Wrap the filter in a `std::shared_ptr<SensitiveWordFilter>` owned by a
namespaced singleton (`sensitive_word_filter::Instance()`). Loader
reads from the source picked in slice A, constructs a fresh filter,
`std::atomic_store` swaps the shared_ptr. Readers
`std::atomic_load` once at the start of their scan, use that
snapshot for the message. Zero locking, zero tearing.

### C. Chat handler wire-up — **M, 1–2 days; blocked on chat system existing**

When `cpp/libs/services/chat/` is built:
- Channel-broadcast path: scan body via `Contains`; on hit, reply
  with `chat error: contains sensitive content` (use the
  `error_tip` + #70/#125 hook so the rejection leaves a triage
  trail). DO NOT silently drop — players need to know their
  message didn't go through.
- DM path: same scan.
- Private/system messages: skip the filter (the system is talking
  to itself).
- Counter: `chat_sensitive_word_hits_total{channel}` Prometheus
  counter so ops can see attack waves vs natural rates.

### D. Algorithm upgrade — **M, when scale demands**

Swap the flat-set scan for Aho-Corasick once: word count > 50k OR
chat messages routinely > 1k bytes (e.g. guild bulletin boards).
The header API stays the same; only the internals change. Don't
do this preemptively.

### E. Bypass / whitelist policy — **discussion needed**

Mods sometimes intentionally need to say a word that matches the
dictionary (e.g. discussing the very thing in a moderator
channel). Decide:
- Is there a `role > moderator` bypass for sensitive-word screening?
- Are some channels exempt entirely (e.g. mod-only)?
- Do we have an "appeal" path where a player can ask a human to
  release a wrongly-blocked message?

Not a code question — needs a product/ops decision before slice C.

---

## Open questions

1. **Source of truth for the dictionary itself.** Hard-coded test
   words for now; the content team should own the production list.
   Where do they edit it — same Excel pipeline as other configs,
   or a separate moderation tool?

2. **Locale separation.** Multi-language deployments will likely
   want per-language word lists (a word that's profanity in one
   language is harmless in another). The filter primitive is
   language-agnostic; slice B's loader decides whether to install
   one filter for all chats or one per language tag.

3. **Storage of the original (uncensored) message** for moderation
   appeal. Redact-on-display vs redact-on-store is a separate
   decision orthogonal to the filter; default to redact-on-display
   (store original, scan + redact on each broadcast) unless ops
   specifically asks for redact-on-store (saves CPU, loses appeal).

---

## References

- `cpp/libs/engine/core/text/sensitive_word_filter.h` — the primitive
  (this slice)
- `proto/chat/chat.proto` — proto definitions awaiting cpp wiring
- `cpp/generated/grpc_client/chat/chat_grpc_client.cpp` — generated
  client stubs (no server-side handler yet)
- todo.md #68 — origin
