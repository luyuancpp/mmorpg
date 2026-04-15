
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



// rewardSnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type rewardSnapshot struct {
    data   []*pb.RewardTable
    kvData map[uint32]*pb.RewardTable
}

type RewardTableManager struct {
    snap *rewardSnapshot
}

var RewardTableManagerInstance = NewRewardTableManager()

func NewRewardTableManager() *RewardTableManager {
    return &RewardTableManager{
        snap: &rewardSnapshot{
            kvData: make(map[uint32]*pb.RewardTable),
        },
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

    snap := &rewardSnapshot{
        kvData: make(map[uint32]*pb.RewardTable, len(container.Data)),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *RewardTableManager) FindAll() []*pb.RewardTable {
    return m.snap.data
}

func (m *RewardTableManager) FindById(id uint32) (*pb.RewardTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *RewardTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}



// ---- Count ----

func (m *RewardTableManager) Count() int {
    return len(m.snap.data)
}



// ---- FindByIds (IN) ----

func (m *RewardTableManager) FindByIds(ids []uint32) []*pb.RewardTable {
    result := make([]*pb.RewardTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *RewardTableManager) RandOne() (*pb.RewardTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}



// ---- Where / First ----

func (m *RewardTableManager) Where(pred func(*pb.RewardTable) bool) []*pb.RewardTable {
    var result []*pb.RewardTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *RewardTableManager) First(pred func(*pb.RewardTable) bool) (*pb.RewardTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

