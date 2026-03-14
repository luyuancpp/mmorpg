package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type MainSceneTableManager struct {
    data []*pb.MainSceneTable
    kvData map[int32]*pb.MainSceneTable
}

func NewMainSceneTableManager() *MainSceneTableManager {
    return &MainSceneTableManager{
        kvData: make(map[int32]*pb.MainSceneTable),
    }
}

func (m *MainSceneTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "mainscene.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.MainSceneTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *MainSceneTableManager) GetById(id int32) (*pb.MainScene, bool) {
    row, ok := m.kvData[id]
    return row, ok
}