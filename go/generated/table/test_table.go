package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type TestTableManager struct {
    data []*pb.Test
    kvData map[int32]*pb.Test
}

func NewTestTableManager() *TestTableManager {
    return &TestTableManager{
        kvData: make(map[int32]*pb.Test),
    }
}

func (m *TestTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "test.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.TestList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *TestTableManager) GetById(id int32) (*pb.Test, bool) {
    row, ok := m.kvData[id]
    return row, ok
}