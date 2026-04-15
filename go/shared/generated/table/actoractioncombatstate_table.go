
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type ActorActionCombatStateTableManager struct {
    data   []*pb.ActorActionCombatStateTable
    kvData map[uint32]*pb.ActorActionCombatStateTable
}

var ActorActionCombatStateTableManagerInstance = NewActorActionCombatStateTableManager()

func NewActorActionCombatStateTableManager() *ActorActionCombatStateTableManager {
    return &ActorActionCombatStateTableManager{
        kvData: make(map[uint32]*pb.ActorActionCombatStateTable),
    }
}

func (m *ActorActionCombatStateTableManager) Load(configDir string, useBinary bool) error {
    var container pb.ActorActionCombatStateTableData

    if useBinary {
        path := filepath.Join(configDir, "actoractioncombatstate.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "actoractioncombatstate.json")
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

func (m *ActorActionCombatStateTableManager) GetAll() []*pb.ActorActionCombatStateTable {
    return m.data
}

func (m *ActorActionCombatStateTableManager) GetById(id uint32) (*pb.ActorActionCombatStateTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

