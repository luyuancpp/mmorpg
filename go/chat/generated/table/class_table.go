package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type ClassTableManager struct {
    data []*pb.ClassTable
    kvData map[int32]*pb.ClassTable
}

func NewClassTableManager() *ClassTableManager {
    return &ClassTableManager{
        kvData: make(map[int32]*pb.ClassTable),
    }
}

func (m *ClassTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "class.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ClassTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ClassTableManager) GetById(id int32) (*pb.Class, bool) {
    row, ok := m.kvData[id]
    return row, ok
}