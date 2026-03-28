
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type MissionTableManager struct {
    data   []*pb.MissionTable
    kvData map[uint32]*pb.MissionTable
    idxCondition_id map[uint32][]*pb.MissionTable
    idxNext_mission_id map[uint32][]*pb.MissionTable
}

var MissionTableManagerInstance = NewMissionTableManager()

func NewMissionTableManager() *MissionTableManager {
    return &MissionTableManager{
        kvData: make(map[uint32]*pb.MissionTable),
        idxCondition_id: make(map[uint32][]*pb.MissionTable),
        idxNext_mission_id: make(map[uint32][]*pb.MissionTable),
    }
}

func (m *MissionTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "mission.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %w", err)
    }

    var container pb.MissionTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        for _, elem := range row.ConditionId {
            m.idxCondition_id[elem] = append(m.idxCondition_id[elem], row)
        }
        for _, elem := range row.NextMissionId {
            m.idxNext_mission_id[elem] = append(m.idxNext_mission_id[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *MissionTableManager) GetById(id uint32) (*pb.MissionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *MissionTableManager) GetByCondition_idIndex(key uint32) []*pb.MissionTable {
    return m.idxCondition_id[key]
}


func (m *MissionTableManager) GetByNext_mission_idIndex(key uint32) []*pb.MissionTable {
    return m.idxNext_mission_id[key]
}

