# Single-Player Rollback — MMO/MOBA Microservice Architecture

> **Discussion Date**: 2025-03-25
> **Context**: MMO/MOBA microservice architecture rollback design discussion

## Problem

In microservice MMO, player data is scattered across many services (bag, quest, mail, currency, guild, friend, ranking, etc.). Rolling back a single player to a previous state requires coordinated restoration across all services.

## Recommended: Application-Level Snapshot + On-Demand Rollback

### Each Service Exposes Export/Import RPC
- `ExportPlayerData`: export full player state for that service (protobuf bytes)
- `ImportPlayerData`: overwrite player state with provided blob (idempotent)

### Snapshot Service
- Periodically (and on key events: login, large transactions, pre-maintenance) calls each service's ExportPlayerData
- Stores composite snapshots: `player_id | snapshot_time | service_name | data_blob`
- Retention: last N days of login snapshots + daily periodic snapshots
- Storage: dedicated table/database, isolated from live data

### Rollback Procedure
1. Kick player offline (mandatory — no rollback while online)
2. Find nearest snapshot ≤ target_timestamp
3. Call ImportPlayerData on each service (respecting dependency order)
4. Write audit log (operator, reason, target time, affected services)
5. Return result to GM/CS tool

### Key Design Points
- **Must be offline**: kick before rollback to avoid write conflicts
- **Idempotency**: ImportPlayerData must be retry-safe
- **External effects are NOT reversed**: mails sent to others, guild donations, trades — only the player's own state rolls back
- **Granularity**: support full rollback AND per-service rollback (e.g., only bag, not quests)
- **Audit trail**: every rollback logged with who/when/why/scope
- **Snapshot isolation**: separate from live DB to prevent corruption cascading
- **Service ordering**: restore in dependency order (generally no circular deps)

### Fallback: Database-Level PITR
- Use MySQL binlog point-in-time recovery to a shadow DB
- Extract target player's rows at target timestamp from shadow
- Import back to live DB
- Heavier operationally; best as disaster-recovery last resort

### Best Practice: Hybrid
- Routine rollbacks → application-level snapshots (fast, precise, automatable)
- Disaster recovery → MySQL PITR (broad coverage, high operational cost)

## Anti-Duplication: Asymmetric Rollback Problem

### The Problem
Player A trades item to Player B. If only A is rolled back, both A and B have the item → duplication.
Applies to ALL bidirectional ops: trade, mail attachments, guild bank, auction house, party loot.

### Solution: Rollback ≠ Time Travel, Rollback = Abnormal Loss Compensation

**Core principle: items/currency with normal transfer records are NOT restored.**

### Required Infrastructure
1. **Item UUID**: every item instance has a global unique ID (SnowFlake). Enables precise tracking.
2. **Transaction Log**: all bidirectional operations write to `transaction_log` via Kafka → DB:
   - tx_id, time, type (trade/mail/auction/guild_bank), from_player, to_player, item_uuid, quantity, currency_delta
3. **Global Item Index**: item_uuid → current_owner lookup (only queried for rollback items)

### Rollback Procedure (Revised)
1. Kick player offline
2. Load snapshot at target time
3. Diff snapshot vs current state
4. For each "missing" item/currency: check transaction_log
   - Has normal transfer record → DO NOT restore (recipient still has it)
   - No transfer record (bug/anomaly) → restore
5. Generate "recoverable items" report for GM review
6. GM approves → execute selective restore
7. Audit log

### Alternative Strategies (Trade-offs)
- **Cascading rollback** (roll back all trade partners): too dangerous, uncontrollable blast radius
- **Soft rollback** (mail compensation only, no DB overwrite): safest, used for routine CS requests
- **Item existence check** (skip restore if UUID still exists in world): simpler but less precise than full tx-log analysis

### Critical Rule
> Single-player rollback is NOT "time reversal" — it is "abnormal loss compensation."
> Any transfer with a valid transaction log entry is excluded from restoration.

## Bug Exploit / Item Duplication — Precision Clawback

### Scenario
Player discovers a bug, exploits it to generate items/currency, then spreads them via trade/mail/auction.

### Handling Layers
1. **Freeze**: ban account immediately to stop exploit
2. **Trace pollution chain**: from transaction_log, follow every dirty item_uuid and currency flow (multi-level: A→B→C→...)
3. **Precision clawback**:
   - Item still on someone → remove by UUID
   - Sold to NPC → deduct equivalent currency
   - On auction house → delist and delete
   - Dirty currency spent on other items → deduct those items or equivalent
4. **Innocent player handling**:
   - Knowing accomplice → clawback + possible ban
   - Unknowing buyer via normal trade → remove item BUT refund what they paid
   - Spread too wide to trace → accept loss if small; emergency maintenance + full server rollback if massive
5. **Nuclear option**: if economy is too polluted → full server rollback (MySQL PITR)

### Four Scenarios Summary
| Scenario | Action | Infrastructure |
|----------|--------|----------------|
| Normal item loss (accidental) | Soft rollback (mail compensation) | Transaction log |
| Structural data corruption | Snapshot restore (real rollback) | Periodic snapshots + Export/Import RPC |
| Bug exploit duplication | Precision clawback (reverse trace) | Transaction log + Item UUID + pollution chain |
| Server-wide economic collapse | Full server rollback | MySQL PITR / full server snapshot |

**All four + deferred clawback (below) depend on: Item UUID (SnowFlake) + Transaction Log.**

## Deferred Clawback (Soft Recovery for Minor Exploits)

### When to Use
- Minor bug exploits where the amount is small, no mass spread, player not malicious
- Pure currency/token duplication (not unique items)
- You want to recover without banning or disrupting the player's experience

### How It Works
1. GM determines player gained X currency via bug
2. Create a `player_debt` record: player owes X of currency_type
3. Every time the player receives that currency (quest reward, trade income, system grant...):
   → System intercepts at the unified `AddCurrency` entry point
   → Deducts min(incoming_amount, remaining_debt) into the debt pool
   → Player receives the remainder
4. Continues until debt is fully repaid

### Data Model
```
player_debt:
  player_id | currency_type | owed_amount | paid_amount | reason | gm_operator | created_at | completed_at
```

### Implementation Key Point
- **Must hook into the single unified currency-add function** — if currency can be added through multiple code paths that bypass the hook, the deduction is easily circumvented
- Pseudocode:
```
AddCurrency(player, type, amount):
  debt = GetPlayerDebt(player, type)
  if debt > 0:
    deduct = min(amount, debt)
    actual_gain = amount - deduct
    ReduceDebt(player, type, deduct)
  else:
    actual_gain = amount
  player.currency[type] += actual_gain
```

### Suitable vs Not Suitable
- **Suitable**: fungible currency (gold, gems, tokens), small amounts, non-malicious players
- **Not suitable**: unique items (must use UUID-based precision clawback), large amounts (debt too large to ever repay naturally), repeat/malicious exploiters (should ban)

### Advantages over Hard Clawback
| | Hard clawback (direct deduction) | Deferred clawback |
|-|----------------------------------|--------------------------|
| Player experience | Sudden loss, likely complaints | Gradual, low-friction |
| Negative balance risk | Can go negative if already spent | Never negative, only deducts from gains |
| Ban required? | Usually no, but may need if ongoing | No, just fix the bug and attach debt |
| Implementation | Simple | Medium — requires unified currency entry point |

## Currency System Architecture Decision

### Conclusion: Currency Lives on C++ Player Entity, Go Only Handles Persistence and GM Operations

**Reason**: combat-time currency operations (buying potions, skill costs, repair fees, etc.) require zero latency. RPC round-trips to a Go service (2–20ms) are unacceptable for hot-path gameplay.

### Architecture
- On login: load currency from DB into `CurrencyComp` (in-memory on C++ node)
- During gameplay: pure in-memory reads/writes
- Periodically and on logout: flush dirty state via Kafka → go/db → MySQL
- Deferred clawback hook is placed inside the C++ `AddCurrency` unified entry point

### Go Service Scope (Offline/Peripheral Only)
- Recharge/top-up delivery
- Web shop purchases
- Mail attachment currency
- GM tools
- All of these ultimately notify the C++ node to execute the actual currency change
