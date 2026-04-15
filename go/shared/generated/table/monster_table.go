
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type MonsterTableManager struct {
    data   []*pb.MonsterTable
    kvData map[uint32]*pb.MonsterTable
}

var MonsterTableManagerInstance = NewMonsterTableManager()

func NewMonsterTableManager() *MonsterTableManager {
    return &MonsterTableManager{
        kvData: make(map[uint32]*pb.MonsterTable),
    }
}

func (m *MonsterTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MonsterTableData

    if useBinary {
        path := filepath.Join(configDir, "monster.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "monster.json")
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

func (m *MonsterTableManager) GetAll() []*pb.MonsterTable {
    return m.data
}

func (m *MonsterTableManager) GetById(id uint32) (*pb.MonsterTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

