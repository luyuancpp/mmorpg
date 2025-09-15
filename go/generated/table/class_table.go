package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type ClassTableManager struct {
    data []*pb.Class
    kvData map[int32]*pb.Class
}

func NewClassTableManager() *ClassTableManager {
    return &ClassTableManager{
        kvData: make(map[int32]*pb.Class),
    }
}

func (m *ClassTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "class.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ClassList
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