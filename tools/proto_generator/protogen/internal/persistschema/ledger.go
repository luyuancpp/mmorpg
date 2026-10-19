// Package persistschema 维护玩家持久 schema 世代台账 proto/persist_schema_gen.txt,并据此产出
// C++ / Go 的世代常量与往返一致性测试用清单。
//
// 设计见 docs/design/mixed-version-persist-field-protection.md §3:每张玩家表消息带覆盖世代
// persist_schema_gen,世代号在这里按「两张玩家表消息可达的字段集合」单调发放;不能被旧写方安全携带的
// 改动(给冻结类型加字段、给已有 oneof 加成员、改类型、删字段不写 reserved、复用字段号)在生成期失败。
//
// 台账与 proto/message_id.txt 同一套路:由生成器维护、提交进仓库,PR 里能看到每次发号。
// 唯一允许的手工修改是负责人在引导期结束时把 state 从 bootstrap 改为 active(§8.1)。
package persistschema

import (
	"bufio"
	"bytes"
	"fmt"
	"strconv"
	"strings"
)

// 台账状态。bootstrap:机制上线、全网换版期间,新增字段一律并入世代 1(不升世代);
// active:每次出现新增字段的生成运行追加一个新世代。
const (
	StateBootstrap = "bootstrap"
	StateActive    = "active"
)

// 字段标签。map 单列出来:map 与 repeated message 的线格式虽然相同,语义(按键合并)不同,改动要被看见。
const (
	LabelOptional = "optional"
	LabelRepeated = "repeated"
	LabelMap      = "map"
)

// Field 是台账里的一条持久字段。身份是 (Message, Number);Name 只为可读,改名不算改动。
type Field struct {
	Message string // 消息全名,不带前导点,例如 "player_database"、"BagAllData.DynamicBagData"
	Number  int32
	Label   string // LabelOptional / LabelRepeated / LabelMap
	Type    string // 标量小写类型名、消息全名、"enum:<全名>" 或 "map<键类型,值类型>"
	Name    string
	Oneof   string // 所属的真实 oneof 名;不在 oneof 里或 proto3 optional 的合成 oneof 为空
}

// Key 返回字段身份 "<消息全名>#<字段号>"。
func (f Field) Key() string {
	return f.Message + "#" + strconv.FormatInt(int64(f.Number), 10)
}

// Entry 是台账里的一行字段记录:字段在哪个世代引入。
type Entry struct {
	Gen   uint32
	Field Field
}

// Ledger 是台账的内存形态。Entries 与 Retired 保持文件顺序,新增只追加,不重排旧行。
type Ledger struct {
	State   string
	Entries []Entry
	Retired []string // 已退役的字段身份(Field.Key),字段号永不复用
}

const ledgerHeader = `# 玩家持久 schema 世代台账。由 proto 生成器维护,不要手改。
# 设计:docs/design/mixed-version-persist-field-protection.md §3。
# state=bootstrap:引导期,新增字段并入世代 1;引导期结束时由负责人改为 state=active,此后每次出现新增字段的
#   生成运行追加一个新世代。只在主干发号:发布分支需要新持久字段时,先合主干拿号,再原样 cherry-pick 本文件的行。
# 字段行:<世代>=<消息全名>#<字段号> <标签> <类型> <字段名>[ oneof=<oneof 名>]
# 退役行:retired=<消息全名>#<字段号>(字段已删除并写了 reserved;字段号永不复用)
`

// ParseLedger 解析台账文本。格式不认识、身份重复都报错并带行号:台账是发号的唯一依据,不能猜。
func ParseLedger(data []byte) (Ledger, error) {
	var l Ledger
	seen := map[string]bool{}
	retired := map[string]bool{}
	scanner := bufio.NewScanner(bytes.NewReader(data))
	scanner.Buffer(make([]byte, 0, 64*1024), 1<<20)
	lineNo := 0
	for scanner.Scan() {
		lineNo++
		line := strings.TrimSpace(strings.TrimSuffix(scanner.Text(), "\r"))
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		key, value, ok := strings.Cut(line, "=")
		if !ok {
			return Ledger{}, fmt.Errorf("台账第 %d 行缺少 '=': %q", lineNo, line)
		}
		switch {
		case key == "state":
			if value != StateBootstrap && value != StateActive {
				return Ledger{}, fmt.Errorf("台账第 %d 行 state 只能是 %s 或 %s: %q", lineNo, StateBootstrap, StateActive, value)
			}
			if l.State != "" {
				return Ledger{}, fmt.Errorf("台账第 %d 行重复声明 state", lineNo)
			}
			l.State = value
		case key == "retired":
			if _, _, err := splitFieldKey(value); err != nil {
				return Ledger{}, fmt.Errorf("台账第 %d 行退役身份不合法: %w", lineNo, err)
			}
			if retired[value] {
				return Ledger{}, fmt.Errorf("台账第 %d 行重复退役 %s", lineNo, value)
			}
			retired[value] = true
			l.Retired = append(l.Retired, value)
		default:
			gen, err := strconv.ParseUint(key, 10, 32)
			if err != nil || gen == 0 {
				return Ledger{}, fmt.Errorf("台账第 %d 行世代号必须是正整数: %q", lineNo, key)
			}
			field, err := parseFieldValue(value)
			if err != nil {
				return Ledger{}, fmt.Errorf("台账第 %d 行: %w", lineNo, err)
			}
			if seen[field.Key()] {
				return Ledger{}, fmt.Errorf("台账第 %d 行重复记录 %s", lineNo, field.Key())
			}
			seen[field.Key()] = true
			l.Entries = append(l.Entries, Entry{Gen: uint32(gen), Field: field})
		}
	}
	if err := scanner.Err(); err != nil {
		return Ledger{}, fmt.Errorf("读取台账失败: %w", err)
	}
	if l.State == "" {
		return Ledger{}, fmt.Errorf("台账缺少 state 行(state=%s 或 state=%s)", StateBootstrap, StateActive)
	}
	// 退役字段的字段行保留(记着它在哪个世代引入);退役行指向台账里不存在的字段,说明台账被手工改坏了。
	for _, key := range l.Retired {
		if !seen[key] {
			return Ledger{}, fmt.Errorf("退役行 %s 在台账里没有对应的字段行", key)
		}
	}
	return l, nil
}

// parseFieldValue 解析字段行等号右侧:"<消息全名>#<字段号> <标签> <类型> <字段名>[ oneof=<名>]"。
func parseFieldValue(value string) (Field, error) {
	parts := strings.Fields(value)
	if len(parts) != 4 && len(parts) != 5 {
		return Field{}, fmt.Errorf("字段行应为 4 或 5 段,实际 %d 段: %q", len(parts), value)
	}
	message, number, err := splitFieldKey(parts[0])
	if err != nil {
		return Field{}, err
	}
	switch parts[1] {
	case LabelOptional, LabelRepeated, LabelMap:
	default:
		return Field{}, fmt.Errorf("未知标签 %q", parts[1])
	}
	f := Field{Message: message, Number: number, Label: parts[1], Type: parts[2], Name: parts[3]}
	if len(parts) == 5 {
		oneof, ok := strings.CutPrefix(parts[4], "oneof=")
		if !ok || oneof == "" {
			return Field{}, fmt.Errorf("第 5 段只能是 oneof=<名>: %q", parts[4])
		}
		f.Oneof = oneof
	}
	return f, nil
}

// splitFieldKey 拆分 "<消息全名>#<字段号>"。
func splitFieldKey(key string) (string, int32, error) {
	message, numberText, ok := strings.Cut(key, "#")
	if !ok || message == "" {
		return "", 0, fmt.Errorf("字段身份应为 <消息全名>#<字段号>: %q", key)
	}
	number, err := strconv.ParseInt(numberText, 10, 32)
	if err != nil || number <= 0 {
		return "", 0, fmt.Errorf("字段号必须是正整数: %q", key)
	}
	return message, int32(number), nil
}

// Format 把台账写回文本。旧行顺序不变,新增行在 Apply 里已追加到末尾,所以 diff 只有追加。
func (l Ledger) Format() []byte {
	var b strings.Builder
	b.WriteString(ledgerHeader)
	b.WriteString("state=")
	b.WriteString(l.State)
	b.WriteString("\n")
	if len(l.Entries) > 0 {
		b.WriteString("\n")
	}
	for _, e := range l.Entries {
		f := e.Field
		fmt.Fprintf(&b, "%d=%s %s %s %s", e.Gen, f.Key(), f.Label, f.Type, f.Name)
		if f.Oneof != "" {
			b.WriteString(" oneof=")
			b.WriteString(f.Oneof)
		}
		b.WriteString("\n")
	}
	if len(l.Retired) > 0 {
		b.WriteString("\n")
	}
	for _, key := range l.Retired {
		b.WriteString("retired=")
		b.WriteString(key)
		b.WriteString("\n")
	}
	return []byte(b.String())
}

// MaxGen 返回台账里最大的世代号;台账为空时为 0。
func (l Ledger) MaxGen() uint32 {
	var maxGen uint32
	for _, e := range l.Entries {
		if e.Gen > maxGen {
			maxGen = e.Gen
		}
	}
	return maxGen
}
