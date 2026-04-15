
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type CooldownTableManager struct {
    data   []*pb.CooldownTable
    kvData map[uint32]*pb.CooldownTable
}

var CooldownTableManagerInstance = NewCooldownTableManager()

func NewCooldownTableManager() *CooldownTableManager {
    return &CooldownTableManager{
        kvData: make(map[uint32]*pb.CooldownTable),
    }
}

func (m *CooldownTableManager) Load(configDir string, useBinary bool) error {
    var container pb.CooldownTableData

    if useBinary {
        path := filepath.Join(configDir, "cooldown.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "cooldown.json")
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

func (m *CooldownTableManager) GetAll() []*pb.CooldownTable {
    return m.data
}

func (m *CooldownTableManager) GetById(id uint32) (*pb.CooldownTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

