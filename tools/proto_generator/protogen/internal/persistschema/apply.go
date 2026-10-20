package persistschema

import (
	"fmt"
	"sort"
)

// Result 是一次比对的结果。Errors 非空时调用方必须让生成失败,且不得写回台账与产物。
type Result struct {
	Ledger  Ledger   // 比对后的台账(只追加)
	Changed bool     // 台账是否需要写回
	Gen     uint32   // 本二进制的世代:台账里最大的世代号,台账为空时为 1
	Errors  []string // 违反可携带性规则(§3.2 R1–R3)或台账与 proto 不一致
	Notes   []string // 不阻断生成、但要让人看见的事项(改名、类型不再可达、疑似字段迁移)
}

// Apply 用当前 proto 的持久字段比对台账,按 §3.1 / §3.2 发号并检查规则。纯函数,不读写文件。
func Apply(old Ledger, s Schema) Result {
	r := Result{Ledger: Ledger{State: old.State}}
	r.Ledger.Entries = append(r.Ledger.Entries, old.Entries...)
	r.Ledger.Retired = append(r.Ledger.Retired, old.Retired...)

	recorded := map[string]Entry{}
	messagesInLedger := map[string]bool{}
	for _, e := range old.Entries {
		recorded[e.Field.Key()] = e
		messagesInLedger[e.Field.Message] = true
	}
	retired := map[string]bool{}
	for _, key := range old.Retired {
		retired[key] = true
	}

	baseline := len(old.Entries) == 0
	if baseline && old.State != StateBootstrap {
		r.Errors = append(r.Errors, "台账没有任何字段行,却不是 state=bootstrap:台账可能被误删或清空;从版本库恢复,不要重新建基线")
		return r
	}
	if old.State == StateBootstrap && old.MaxGen() > 1 {
		r.Errors = append(r.Errors, fmt.Sprintf("台账是 state=bootstrap,却已有世代 %d 的字段:引导期只有世代 1,state 不能从 active 改回 bootstrap", old.MaxGen()))
		return r
	}
	newGen := old.MaxGen() + 1
	if old.State == StateBootstrap {
		newGen = 1
	}

	keys := make([]string, 0, len(s.Fields))
	for key := range s.Fields {
		keys = append(keys, key)
	}
	sort.Slice(keys, func(i, j int) bool {
		a, b := s.Fields[keys[i]], s.Fields[keys[j]]
		if a.Message != b.Message {
			return a.Message < b.Message
		}
		return a.Number < b.Number
	})

	addedIn := map[string]bool{}
	var added []Entry
	for _, key := range keys {
		cur := s.Fields[key]
		if retired[key] {
			r.Errors = append(r.Errors, fmt.Sprintf("%s(%s)复用了已退役的字段号;上线后字段号不得复用(AGENTS §4.3),换一个新号", key, cur.Name))
			continue
		}
		if prev, ok := recorded[key]; ok {
			if prev.Field.Label != cur.Label || prev.Field.Type != cur.Type || prev.Field.Oneof != cur.Oneof {
				r.Errors = append(r.Errors, fmt.Sprintf("%s(%s)的定义由「%s」改成了「%s」;持久字段不得改标签、类型或 oneof 归属(R3),"+
					"请写 reserved 后用新字段号承载新定义", key, cur.Name, describe(prev.Field), describe(cur)))
			}
			if prev.Field.Name != cur.Name {
				r.Notes = append(r.Notes, fmt.Sprintf("%s 改名 %s → %s;线格式不变,台账保留原名", key, prev.Field.Name, cur.Name))
			}
			continue
		}
		if !baseline {
			if s.Frozen[cur.Message] && messagesInLedger[cur.Message] {
				r.Errors = append(r.Errors, fmt.Sprintf("%s(%s)加在冻结类型 %s 上;冻结类型由手写代码重建、不携带未知字段,旧写方会把它抹掉(R1)。"+
					"把数据放进会被整份携带的类型(物品数据进 ItemComp,任务进度进 MissionListComp),"+
					"或先为该类型实现运行时携带并纳入往返一致性测试再解冻", key, cur.Name, cur.Message))
				continue
			}
			if cur.Oneof != "" && oneofRecorded(old.Entries, cur.Message, cur.Oneof) {
				r.Errors = append(r.Errors, fmt.Sprintf("%s(%s)给已有的持久 oneof %s.%s 加了新成员;旧写方设置别的成员时会与携带回来的新成员冲突(R2)。"+
					"把字段放到 oneof 之外,或走破坏性变更流程", key, cur.Name, cur.Message, cur.Oneof))
				continue
			}
		}
		added = append(added, Entry{Gen: newGen, Field: cur})
		addedIn[cur.Message] = true
	}

	retiredIn := map[string]bool{}
	for _, e := range old.Entries {
		key := e.Field.Key()
		if retired[key] {
			continue
		}
		if _, ok := s.Fields[key]; ok {
			continue
		}
		if !s.Reachable[e.Field.Message] {
			r.Notes = append(r.Notes, fmt.Sprintf("%s(%s)所在类型已不再从玩家表可达,台账行保留不动", key, e.Field.Name))
			continue
		}
		if !s.isReserved(e.Field.Message, e.Field.Number) {
			r.Errors = append(r.Errors, fmt.Sprintf("持久字段 %s(%s)被删除但没有写 reserved %d;上线后字段号不得复用,删字段必须写 reserved(AGENTS §4.3)",
				key, e.Field.Name, e.Field.Number))
			continue
		}
		r.Ledger.Retired = append(r.Ledger.Retired, key)
		retiredIn[e.Field.Message] = true
		r.Changed = true
	}

	if len(r.Errors) > 0 {
		return r
	}
	for message := range addedIn {
		if retiredIn[message] {
			r.Notes = append(r.Notes, fmt.Sprintf("%s 在同一次生成里既删字段又加字段:如果是字段迁移(提升为列、拆分、换存储位置),"+
				"必须走 expand / contract(R4),不能直接搬", message))
		}
	}
	sort.Strings(r.Notes)
	if len(added) > 0 {
		r.Ledger.Entries = append(r.Ledger.Entries, added...)
		r.Changed = true
	}
	r.Gen = r.Ledger.MaxGen()
	if r.Gen == 0 {
		r.Gen = 1
	}
	return r
}

func oneofRecorded(entries []Entry, message, oneof string) bool {
	for _, e := range entries {
		if e.Field.Message == message && e.Field.Oneof == oneof {
			return true
		}
	}
	return false
}

func describe(f Field) string {
	s := f.Label + " " + f.Type
	if f.Oneof != "" {
		s += " oneof=" + f.Oneof
	}
	return s
}
