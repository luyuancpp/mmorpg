package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type ActorActionStateTableManager struct {
    data []*pb.ActorActionState
    kvData map[int32]*pb.ActorActionState
}

func NewActorActionStateTableManager() *ActorActionStateTableManager {
    return &ActorActionStateTableManager{
        kvData: make(map[int32]*pb.ActorActionState),
    }
}

func (m *ActorActionStateTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "actoractionstate.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ActorActionStateList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ActorActionStateTableManager) GetById(id int32) (*pb.ActorActionState, bool) {
    row, ok := m.kvData[id]
    return row, ok
}