package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type GlobalVariableTableManager struct {
    data []*pb.GlobalVariable
    kvData map[int32]*pb.GlobalVariable
}

func NewGlobalVariableTableManager() *GlobalVariableTableManager {
    return &GlobalVariableTableManager{
        kvData: make(map[int32]*pb.GlobalVariable),
    }
}

func (m *GlobalVariableTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "globalvariable.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.GlobalVariableList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *GlobalVariableTableManager) GetById(id int32) (*pb.GlobalVariable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}