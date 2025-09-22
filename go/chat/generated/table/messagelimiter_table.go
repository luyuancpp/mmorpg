package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type MessageLimiterTableManager struct {
    data []*pb.MessageLimiterTable
    kvData map[int32]*pb.MessageLimiterTable
}

func NewMessageLimiterTableManager() *MessageLimiterTableManager {
    return &MessageLimiterTableManager{
        kvData: make(map[int32]*pb.MessageLimiterTable),
    }
}

func (m *MessageLimiterTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "messagelimiter.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.MessageLimiterTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *MessageLimiterTableManager) GetById(id int32) (*pb.MessageLimiter, bool) {
    row, ok := m.kvData[id]
    return row, ok
}