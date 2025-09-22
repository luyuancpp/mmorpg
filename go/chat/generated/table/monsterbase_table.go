package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type MonsterBaseTableManager struct {
    data []*pb.MonsterBaseTable
    kvData map[int32]*pb.MonsterBaseTable
}

func NewMonsterBaseTableManager() *MonsterBaseTableManager {
    return &MonsterBaseTableManager{
        kvData: make(map[int32]*pb.MonsterBaseTable),
    }
}

func (m *MonsterBaseTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "monsterbase.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.MonsterBaseTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
    }

    m.data = container.Data
    return nil
}

func (m *MonsterBaseTableManager) GetById(id int32) (*pb.MonsterBase, bool) {
    row, ok := m.kvData[id]
    return row, ok
}