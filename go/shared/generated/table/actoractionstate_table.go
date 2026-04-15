package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "shared/generated/pb/table"
)

var ActorActionStateTableManagerInstance = NewActorActionStateTableManager()


type ActorActionStateTableManager struct {
    data []*pb.ActorActionStateTable
    kvData map[uint32]*pb.ActorActionStateTable
}

func NewActorActionStateTableManager() *ActorActionStateTableManager {
    return &ActorActionStateTableManager{
        kvData: make(map[uint32]*pb.ActorActionStateTable),
    }
}

func (m *ActorActionStateTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "actoractionstate.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.ActorActionStateTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *ActorActionStateTableManager) GetById(id uint32) (*pb.ActorActionStateTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}