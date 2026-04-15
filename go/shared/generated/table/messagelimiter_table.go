
package table

import (
    "fmt"
    "math/rand/v2"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)



// messagelimiterSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type messagelimiterSnapshot struct {
    data   []*pb.MessageLimiterTable
    kvData map[uint32]*pb.MessageLimiterTable
}

type MessageLimiterTableManager struct {
    snap *messagelimiterSnapshot
}

var MessageLimiterTableManagerInstance = NewMessageLimiterTableManager()

func NewMessageLimiterTableManager() *MessageLimiterTableManager {
    return &MessageLimiterTableManager{
        snap: &messagelimiterSnapshot{
            kvData: make(map[uint32]*pb.MessageLimiterTable),
        },
    }
}

func (m *MessageLimiterTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MessageLimiterTableData

    if useBinary {
        path := filepath.Join(configDir, "messagelimiter.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "messagelimiter.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    snap := &messagelimiterSnapshot{
        kvData: make(map[uint32]*pb.MessageLimiterTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *MessageLimiterTableManager) FindAll() []*pb.MessageLimiterTable {
    return m.snap.data
}

func (m *MessageLimiterTableManager) FindById(id uint32) (*pb.MessageLimiterTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *MessageLimiterTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *MessageLimiterTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *MessageLimiterTableManager) FindByIds(ids []uint32) []*pb.MessageLimiterTable {
    result := make([]*pb.MessageLimiterTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *MessageLimiterTableManager) RandOne() (*pb.MessageLimiterTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *MessageLimiterTableManager) Where(pred func(*pb.MessageLimiterTable) bool) []*pb.MessageLimiterTable {
    var result []*pb.MessageLimiterTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *MessageLimiterTableManager) First(pred func(*pb.MessageLimiterTable) bool) (*pb.MessageLimiterTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

