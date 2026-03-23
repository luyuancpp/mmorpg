package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "login/generated/pb/table"
)

var ConditionTableManagerInstance = NewConditionTableManager()


type ConditionTableManager struct {
    data []*pb.ConditionTable
    kvData map[uint32]*pb.ConditionTable
}

func NewConditionTableManager() *ConditionTableManager {
    return &ConditionTableManager{
        kvData: make(map[uint32]*pb.ConditionTable),
    }
}

func (m *ConditionTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "condition.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ConditionTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ConditionTableManager) GetById(id uint32) (*pb.ConditionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}