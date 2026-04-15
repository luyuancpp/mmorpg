
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

func (m *RewardTableManager) FindAll() []*pb.RewardTable {
    return m.data
}

func (m *RewardTableManager) FindById(id uint32) (*pb.RewardTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *RewardTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *RewardTableManager) Count() int {
    return len(m.data)
}



// ---- FindByIds (IN) ----

func (m *RewardTableManager) FindByIds(ids []uint32) []*pb.RewardTable {
    result := make([]*pb.RewardTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *RewardTableManager) RandOne() (*pb.RewardTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *RewardTableManager) Where(pred func(*pb.RewardTable) bool) []*pb.RewardTable {
    var result []*pb.RewardTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *RewardTableManager) First(pred func(*pb.RewardTable) bool) (*pb.RewardTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

