
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type DungeonTableManager struct {
    data   []*pb.DungeonTable
    kvData map[uint32]*pb.DungeonTable
}

var DungeonTableManagerInstance = NewDungeonTableManager()

func NewDungeonTableManager() *DungeonTableManager {
    return &DungeonTableManager{
        kvData: make(map[uint32]*pb.DungeonTable),
    }
}

func (m *DungeonTableManager) Load(configDir string, useBinary bool) error {
    var container pb.DungeonTableData

    if useBinary {
        path := filepath.Join(configDir, "dungeon.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "dungeon.json")
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

func (m *DungeonTableManager) GetAll() []*pb.DungeonTable {
    return m.data
}

func (m *DungeonTableManager) GetById(id uint32) (*pb.DungeonTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


// FK: scene_id → BaseScene.id
