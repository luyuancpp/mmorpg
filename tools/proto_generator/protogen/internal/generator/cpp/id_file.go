package cpp

import (
	"fmt"
	"strconv"
	"strings"

	"protogen/internal"
)

// event_id.txt 与 message_id.txt 共用同一种行格式:活行 `N=<名>`,墓碑行 `N=reserved:<原名>`
// (前缀见 internal.ReservedEventIdPrefix)。本文件只放两者共用的行解析与重号校验;
// 发号规则、墓碑上限与畸形行的处置由 event_id.go / service_register_info.go 各自决定。

// idFileEntry 是 id 文件的一行:活行 `N=<名>`,或墓碑 `N=reserved:<原名>`。
type idFileEntry struct {
	Id       uint64
	Name     string // 活行为当前名;墓碑为被删条目的原名
	Reserved bool
}

// parseIdFileLine 解析 id 文件的一行(纯函数)。
// ok=false:不是 `N=...` 的形状,由调用方决定告警跳过还是中止;
// err 非空:号不是十进制整数,调用方应中止生成。
func parseIdFileLine(line string) (entry idFileEntry, ok bool, err error) {
	parts := strings.Split(line, "=")
	if len(parts) != 2 {
		return idFileEntry{}, false, nil
	}

	id, err := strconv.ParseUint(strings.TrimSpace(parts[0]), 10, 64)
	if err != nil {
		return idFileEntry{}, false, err
	}

	name := strings.TrimSpace(parts[1])
	if originalName, isReserved := strings.CutPrefix(name, internal.ReservedEventIdPrefix); isReserved {
		return idFileEntry{Id: id, Name: strings.TrimSpace(originalName), Reserved: true}, true, nil
	}
	return idFileEntry{Id: id, Name: name}, true, nil
}

// idFileState 是 id 文件的内存视图。
type idFileState struct {
	Active   map[string]uint64 // 活行:名 → 号
	Reserved map[uint64]string // 墓碑:号 → 被删条目的原名
	MaxId    uint64            // 全部行(含墓碑)里的最大号
}

func newIdFileState() *idFileState {
	return &idFileState{
		Active:   map[string]uint64{},
		Reserved: map[uint64]string{},
	}
}

// add 登记一行。同一个号出现两次(包括一个号既是活号又是墓碑),或同一个活名出现两次,
// 都说明文件被手改坏了:照旧生成会让两个条目共用一个号,或让某个号在下一轮被当成空洞回收。
// 返回错误,由调用方中止生成(fail-closed)。墓碑名与活名相同允许共存(删掉后又以同名加回)。
func (s *idFileState) add(entry idFileEntry) error {
	if originalName, dup := s.Reserved[entry.Id]; dup {
		return fmt.Errorf("id %d listed twice: already reserved for %q", entry.Id, originalName)
	}
	for name, id := range s.Active {
		if id == entry.Id {
			return fmt.Errorf("id %d listed twice: already used by %q", entry.Id, name)
		}
	}

	if entry.Reserved {
		s.Reserved[entry.Id] = entry.Name
	} else {
		if prevId, dup := s.Active[entry.Name]; dup {
			return fmt.Errorf("name %q listed twice: ids %d and %d", entry.Name, prevId, entry.Id)
		}
		s.Active[entry.Name] = entry.Id
	}

	if s.MaxId < entry.Id {
		s.MaxId = entry.Id
	}
	return nil
}

// formatIdNames 把号按入参顺序拼成 `号=名, 号=名`,供错误信息列出涉及的条目。
func formatIdNames(ids []uint64, names map[uint64]string) string {
	parts := make([]string, 0, len(ids))
	for _, id := range ids {
		parts = append(parts, fmt.Sprintf("%d=%s", id, names[id]))
	}
	return strings.Join(parts, ", ")
}
