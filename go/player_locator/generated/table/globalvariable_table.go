package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "player_locator/generated/pb/table"
)
var GlobalVariableTableManagerInstance = NewGlobalVariableTableManager()


type GlobalVariableTableManager struct {
    data []*pb.GlobalVariableTable
    kvData map[uint32]*pb.GlobalVariableTable
}

func NewGlobalVariableTableManager() *GlobalVariableTableManager {
    return &GlobalVariableTableManager{
        kvData: make(map[uint32]*pb.GlobalVariableTable),
    }
}

func (m *GlobalVariableTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "globalvariable.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.GlobalVariableTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *GlobalVariableTableManager) GetById(id uint32) (*pb.GlobalVariableTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}