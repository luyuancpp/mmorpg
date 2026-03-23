package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "login/generated/pb/table"
)

var BuffTableManagerInstance = NewBuffTableManager()


type BuffTableManager struct {
    data []*pb.BuffTable
    kvData map[uint32]*pb.BuffTable
}

func NewBuffTableManager() *BuffTableManager {
    return &BuffTableManager{
        kvData: make(map[uint32]*pb.BuffTable),
    }
}

func (m *BuffTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "buff.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.BuffTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *BuffTableManager) GetById(id uint32) (*pb.BuffTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}