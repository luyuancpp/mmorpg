
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



type RewardTableManager struct {
    data   []*pb.RewardTable
    kvData map[uint32]*pb.RewardTable
}

var RewardTableManagerInstance = NewRewardTableManager()

func NewRewardTableManager() *RewardTableManager {
    return &RewardTableManager{
        kvData: make(map[uint32]*pb.RewardTable),
    }
}

func (m *RewardTableManager) Load(configDir string, useBinary bool) error {
    var container pb.RewardTableData

    if useBinary {
        path := filepath.Join(configDir, "reward.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "reward.json")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := protojson.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse json: %w", err)
        }
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *RewardTableManager) GetAll() []*pb.RewardTable {
    return m.data
}

func (m *RewardTableManager) GetById(id uint32) (*pb.RewardTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Has / Exists ----

func (m *RewardTableManager) HasId(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Len / Count ----

func (m *RewardTableManager) Len() int {
    return len(m.data)
}



// ---- Batch Lookup (IN) ----

func (m *RewardTableManager) GetByIds(ids []uint32) []*pb.RewardTable {
    result := make([]*pb.RewardTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- Random ----

func (m *RewardTableManager) GetRandom() (*pb.RewardTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Filter / FindFirst ----

func (m *RewardTableManager) Filter(pred func(*pb.RewardTable) bool) []*pb.RewardTable {
    var result []*pb.RewardTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *RewardTableManager) FindFirst(pred func(*pb.RewardTable) bool) (*pb.RewardTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

