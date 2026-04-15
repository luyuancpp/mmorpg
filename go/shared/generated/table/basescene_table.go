
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type BaseSceneTableManager struct {
    data   []*pb.BaseSceneTable
    kvData map[uint32]*pb.BaseSceneTable
}

var BaseSceneTableManagerInstance = NewBaseSceneTableManager()

func NewBaseSceneTableManager() *BaseSceneTableManager {
    return &BaseSceneTableManager{
        kvData: make(map[uint32]*pb.BaseSceneTable),
    }
}

func (m *BaseSceneTableManager) Load(configDir string, useBinary bool) error {
    var container pb.BaseSceneTableData

    if useBinary {
        path := filepath.Join(configDir, "basescene.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "basescene.json")
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

func (m *BaseSceneTableManager) GetAll() []*pb.BaseSceneTable {
    return m.data
}

func (m *BaseSceneTableManager) GetById(id uint32) (*pb.BaseSceneTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

