
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type ItemTableManager struct {
    data   []*pb.ItemTable
    kvData map[uint32]*pb.ItemTable
}

var ItemTableManagerInstance = NewItemTableManager()

func NewItemTableManager() *ItemTableManager {
    return &ItemTableManager{
        kvData: make(map[uint32]*pb.ItemTable),
    }
}

func (m *ItemTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ItemTableData

    if useBinary {
        path := filepath.Join(configDir, "item.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "item.json")
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

func (m *ItemTableManager) GetAll() []*pb.ItemTable {
    return m.data
}

func (m *ItemTableManager) GetById(id uint32) (*pb.ItemTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

