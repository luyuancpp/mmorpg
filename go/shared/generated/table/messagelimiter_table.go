
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)

type MessageLimiterTableManager struct {
    data   []*pb.MessageLimiterTable
    kvData map[uint32]*pb.MessageLimiterTable
}

var MessageLimiterTableManagerInstance = NewMessageLimiterTableManager()

func NewMessageLimiterTableManager() *MessageLimiterTableManager {
    return &MessageLimiterTableManager{
        kvData: make(map[uint32]*pb.MessageLimiterTable),
    }
}

func (m *MessageLimiterTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MessageLimiterTableData

    if useBinary {
        path := filepath.Join(configDir, "messagelimiter.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "messagelimiter.json")
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

func (m *MessageLimiterTableManager) GetAll() []*pb.MessageLimiterTable {
    return m.data
}

func (m *MessageLimiterTableManager) GetById(id uint32) (*pb.MessageLimiterTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

