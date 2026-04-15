
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type ActorActionStateTableManager struct {
    data   []*pb.ActorActionStateTable
    kvData map[uint32]*pb.ActorActionStateTable
}

var ActorActionStateTableManagerInstance = NewActorActionStateTableManager()

func NewActorActionStateTableManager() *ActorActionStateTableManager {
    return &ActorActionStateTableManager{
        kvData: make(map[uint32]*pb.ActorActionStateTable),
    }
}

func (m *ActorActionStateTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ActorActionStateTableData

    if useBinary {
        path := filepath.Join(configDir, "actoractionstate.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "actoractionstate.json")
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

func (m *ActorActionStateTableManager) GetAll() []*pb.ActorActionStateTable {
    return m.data
}

func (m *ActorActionStateTableManager) GetById(id uint32) (*pb.ActorActionStateTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

