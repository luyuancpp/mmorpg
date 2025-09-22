package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type CooldownTableManager struct {
    data []*pb.CooldownTable
    kvData map[int32]*pb.CooldownTable
}

func NewCooldownTableManager() *CooldownTableManager {
    return &CooldownTableManager{
        kvData: make(map[int32]*pb.CooldownTable),
    }
}

func (m *CooldownTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "cooldown.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.CooldownTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *CooldownTableManager) GetById(id int32) (*pb.Cooldown, bool) {
    row, ok := m.kvData[id]
    return row, ok
}