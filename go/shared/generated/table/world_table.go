
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type WorldTableManager struct {
    data   []*pb.WorldTable
    kvData map[uint32]*pb.WorldTable
}

var WorldTableManagerInstance = NewWorldTableManager()

func NewWorldTableManager() *WorldTableManager {
    return &WorldTableManager{
        kvData: make(map[uint32]*pb.WorldTable),
    }
}

func (m *WorldTableManager) Load(configDir string, useBinary bool) error {
    var container pb.WorldTableData

    if useBinary {
        path := filepath.Join(configDir, "world.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "world.json")
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

func (m *WorldTableManager) GetAll() []*pb.WorldTable {
    return m.data
}

func (m *WorldTableManager) GetById(id uint32) (*pb.WorldTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


// FK: scene_id → BaseScene.id
