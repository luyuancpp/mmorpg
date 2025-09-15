package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type ItemTableManager struct {
    data []*pb.Item
    kvData map[int32]*pb.Item
}

func NewItemTableManager() *ItemTableManager {
    return &ItemTableManager{
        kvData: make(map[int32]*pb.Item),
    }
}

func (m *ItemTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "item.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ItemList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ItemTableManager) GetById(id int32) (*pb.Item, bool) {
    row, ok := m.kvData[id]
    return row, ok
}