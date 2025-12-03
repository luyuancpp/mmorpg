package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type SceneTableManager struct {
    data []*pb.SceneTable
    kvData map[int32]*pb.SceneTable
}

func NewSceneTableManager() *SceneTableManager {
    return &SceneTableManager{
        kvData: make(map[int32]*pb.SceneTable),
    }
}

func (m *SceneTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "scene.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.SceneTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *SceneTableManager) GetById(id int32) (*pb.Scene, bool) {
    row, ok := m.kvData[id]
    return row, ok
}