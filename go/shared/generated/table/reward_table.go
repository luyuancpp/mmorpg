
package table

import (
    "fmt"
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

