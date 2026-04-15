
package table

import (
    "fmt"
    "math/rand/v2"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    "google.golang.org/protobuf/proto"
    pb "shared/generated/pb/table"
)



type MissionTableManager struct {
    data   []*pb.MissionTable
    kvData map[uint32]*pb.MissionTable
    idxCondition_id map[uint32][]*pb.MissionTable
    idxNext_mission_id map[uint32][]*pb.MissionTable
    idxTarget_count map[uint32][]*pb.MissionTable
}

var MissionTableManagerInstance = NewMissionTableManager()

func NewMissionTableManager() *MissionTableManager {
    return &MissionTableManager{
        kvData: make(map[uint32]*pb.MissionTable),
        idxCondition_id: make(map[uint32][]*pb.MissionTable),
        idxNext_mission_id: make(map[uint32][]*pb.MissionTable),
        idxTarget_count: make(map[uint32][]*pb.MissionTable),
    }
}

func (m *MissionTableManager) Load(configDir string, useBinary bool) error {
    var container pb.MissionTableData

    if useBinary {
        path := filepath.Join(configDir, "mission.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "mission.json")
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
        for _, elem := range row.ConditionId {
            m.idxCondition_id[elem] = append(m.idxCondition_id[elem], row)
        }
        for _, elem := range row.NextMissionId {
            m.idxNext_mission_id[elem] = append(m.idxNext_mission_id[elem], row)
        }
        for _, elem := range row.TargetCount {
            m.idxTarget_count[elem] = append(m.idxTarget_count[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *MissionTableManager) FindAll() []*pb.MissionTable {
    return m.data
}

func (m *MissionTableManager) FindById(id uint32) (*pb.MissionTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}


func (m *MissionTableManager) FindByCondition_idIndex(key uint32) []*pb.MissionTable {
    return m.idxCondition_id[key]
}


func (m *MissionTableManager) FindByNext_mission_idIndex(key uint32) []*pb.MissionTable {
    return m.idxNext_mission_id[key]
}


func (m *MissionTableManager) FindByTarget_countIndex(key uint32) []*pb.MissionTable {
    return m.idxTarget_count[key]
}



// ---- Exists ----

func (m *MissionTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *MissionTableManager) Count() int {
    return len(m.data)
}


func (m *MissionTableManager) CountByCondition_idIndex(key uint32) int {
    return len(m.idxCondition_id[key])
}


func (m *MissionTableManager) CountByNext_mission_idIndex(key uint32) int {
    return len(m.idxNext_mission_id[key])
}


func (m *MissionTableManager) CountByTarget_countIndex(key uint32) int {
    return len(m.idxTarget_count[key])
}



// ---- FindByIds (IN) ----

func (m *MissionTableManager) FindByIds(ids []uint32) []*pb.MissionTable {
    result := make([]*pb.MissionTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *MissionTableManager) RandOne() (*pb.MissionTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *MissionTableManager) Where(pred func(*pb.MissionTable) bool) []*pb.MissionTable {
    var result []*pb.MissionTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *MissionTableManager) First(pred func(*pb.MissionTable) bool) (*pb.MissionTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

