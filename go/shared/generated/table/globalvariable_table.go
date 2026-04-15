
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



type GlobalVariableTableManager struct {
    data   []*pb.GlobalVariableTable
    kvData map[uint32]*pb.GlobalVariableTable
}

var GlobalVariableTableManagerInstance = NewGlobalVariableTableManager()

func NewGlobalVariableTableManager() *GlobalVariableTableManager {
    return &GlobalVariableTableManager{
        kvData: make(map[uint32]*pb.GlobalVariableTable),
    }
}

func (m *GlobalVariableTableManager) Load(configDir string, useBinary bool) error {
    var container pb.GlobalVariableTableData

    if useBinary {
        path := filepath.Join(configDir, "globalvariable.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "globalvariable.json")
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

func (m *GlobalVariableTableManager) FindAll() []*pb.GlobalVariableTable {
    return m.data
}

func (m *GlobalVariableTableManager) FindById(id uint32) (*pb.GlobalVariableTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}



// ---- Exists ----

func (m *GlobalVariableTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}



// ---- Count ----

func (m *GlobalVariableTableManager) Count() int {
    return len(m.data)
}



// ---- FindByIds (IN) ----

func (m *GlobalVariableTableManager) FindByIds(ids []uint32) []*pb.GlobalVariableTable {
    result := make([]*pb.GlobalVariableTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *GlobalVariableTableManager) RandOne() (*pb.GlobalVariableTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}



// ---- Where / First ----

func (m *GlobalVariableTableManager) Where(pred func(*pb.GlobalVariableTable) bool) []*pb.GlobalVariableTable {
    var result []*pb.GlobalVariableTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *GlobalVariableTableManager) First(pred func(*pb.GlobalVariableTable) bool) (*pb.GlobalVariableTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

