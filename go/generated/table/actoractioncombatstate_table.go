package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type ActorActionCombatStateTableManager struct {
    data []*pb.ActorActionCombatState
    kvData map[int32]*pb.ActorActionCombatState
}

func NewActorActionCombatStateTableManager() *ActorActionCombatStateTableManager {
    return &ActorActionCombatStateTableManager{
        kvData: make(map[int32]*pb.ActorActionCombatState),
    }
}

func (m *ActorActionCombatStateTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "actoractioncombatstate.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ActorActionCombatStateList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ActorActionCombatStateTableManager) GetById(id int32) (*pb.ActorActionCombatState, bool) {
    row, ok := m.kvData[id]
    return row, ok
}