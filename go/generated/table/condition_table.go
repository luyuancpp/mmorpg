package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type ConditionTableManager struct {
    data []*pb.Condition
    kvData map[int32]*pb.Condition
}

func NewConditionTableManager() *ConditionTableManager {
    return &ConditionTableManager{
        kvData: make(map[int32]*pb.Condition),
    }
}

func (m *ConditionTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "condition.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ConditionList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ConditionTableManager) GetById(id int32) (*pb.Condition, bool) {
    row, ok := m.kvData[id]
    return row, ok
}