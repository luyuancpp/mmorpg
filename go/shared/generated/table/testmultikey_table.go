
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



type TestMultiKeyTableManager struct {
    data   []*pb.TestMultiKeyTable
    kvData map[uint32]*pb.TestMultiKeyTable
    kvString_keyData map[string]*pb.TestMultiKeyTable
    kvUint32_keyData map[uint32]*pb.TestMultiKeyTable
    kvInt32_keyData map[int32]*pb.TestMultiKeyTable
    kvM_string_keyData map[string][]*pb.TestMultiKeyTable
    kvM_uint32_keyData map[uint32][]*pb.TestMultiKeyTable
    kvM_int32_keyData map[int32][]*pb.TestMultiKeyTable
    idxEffect map[uint32][]*pb.TestMultiKeyTable
}

var TestMultiKeyTableManagerInstance = NewTestMultiKeyTableManager()

func NewTestMultiKeyTableManager() *TestMultiKeyTableManager {
    return &TestMultiKeyTableManager{
        kvData: make(map[uint32]*pb.TestMultiKeyTable),
        kvString_keyData: make(map[string]*pb.TestMultiKeyTable),
        kvUint32_keyData: make(map[uint32]*pb.TestMultiKeyTable),
        kvInt32_keyData: make(map[int32]*pb.TestMultiKeyTable),
        kvM_string_keyData: make(map[string][]*pb.TestMultiKeyTable),
        kvM_uint32_keyData: make(map[uint32][]*pb.TestMultiKeyTable),
        kvM_int32_keyData: make(map[int32][]*pb.TestMultiKeyTable),
        idxEffect: make(map[uint32][]*pb.TestMultiKeyTable),
    }
}

func (m *TestMultiKeyTableManager) Load(configDir string, useBinary bool) error {
    var container pb.TestMultiKeyTableData

    if useBinary {
        path := filepath.Join(configDir, "testmultikey.pb")
        raw, err := os.ReadFile(path)
        if err != nil {
            return fmt.Errorf("failed to read file: %w", err)
        }
        if err := proto.Unmarshal(raw, &container); err != nil {
            return fmt.Errorf("failed to parse binary: %w", err)
        }
    } else {
        path := filepath.Join(configDir, "testmultikey.json")
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
        m.kvString_keyData[row.StringKey] = row
        m.kvUint32_keyData[row.Uint32Key] = row
        m.kvInt32_keyData[row.Int32Key] = row
        m.kvM_string_keyData[row.MStringKey] = append(m.kvM_string_keyData[row.MStringKey], row)
        m.kvM_uint32_keyData[row.MUint32Key] = append(m.kvM_uint32_keyData[row.MUint32Key], row)
        m.kvM_int32_keyData[row.MInt32Key] = append(m.kvM_int32_keyData[row.MInt32Key], row)
        for _, elem := range row.Effect {
            m.idxEffect[elem] = append(m.idxEffect[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *TestMultiKeyTableManager) FindAll() []*pb.TestMultiKeyTable {
    return m.data
}

func (m *TestMultiKeyTableManager) FindById(id uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByString_key(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvString_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByUint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvUint32_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByInt32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvInt32_keyData[key]
    return row, ok
}


func (m *TestMultiKeyTableManager) FindByM_string_key(key string) []*pb.TestMultiKeyTable {
    return m.kvM_string_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByM_uint32_key(key uint32) []*pb.TestMultiKeyTable {
    return m.kvM_uint32_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByM_int32_key(key int32) []*pb.TestMultiKeyTable {
    return m.kvM_int32_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByEffectIndex(key uint32) []*pb.TestMultiKeyTable {
    return m.idxEffect[key]
}



// ---- Exists ----

func (m *TestMultiKeyTableManager) Exists(id uint32) bool {
    _, ok := m.kvData[id]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByString_key(key string) bool {
    _, ok := m.kvString_keyData[key]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByUint32_key(key uint32) bool {
    _, ok := m.kvUint32_keyData[key]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByInt32_key(key int32) bool {
    _, ok := m.kvInt32_keyData[key]
    return ok
}



// ---- Count ----

func (m *TestMultiKeyTableManager) Count() int {
    return len(m.data)
}

func (m *TestMultiKeyTableManager) CountByM_string_key(key string) int {
    return len(m.kvM_string_keyData[key])
}

func (m *TestMultiKeyTableManager) CountByM_uint32_key(key uint32) int {
    return len(m.kvM_uint32_keyData[key])
}

func (m *TestMultiKeyTableManager) CountByM_int32_key(key int32) int {
    return len(m.kvM_int32_keyData[key])
}


func (m *TestMultiKeyTableManager) CountByEffectIndex(key uint32) int {
    return len(m.idxEffect[key])
}



// ---- FindByIds (IN) ----

func (m *TestMultiKeyTableManager) FindByIds(ids []uint32) []*pb.TestMultiKeyTable {
    result := make([]*pb.TestMultiKeyTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *TestMultiKeyTableManager) RandOne() (*pb.TestMultiKeyTable, bool) {
    if len(m.data) == 0 {
        return nil, false
    }
    return m.data[rand.IntN(len(m.data))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_string_key(key string) (*pb.TestMultiKeyTable, bool) {
    rows := m.kvM_string_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_uint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    rows := m.kvM_uint32_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_int32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    rows := m.kvM_int32_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}



// ---- Where / First ----

func (m *TestMultiKeyTableManager) Where(pred func(*pb.TestMultiKeyTable) bool) []*pb.TestMultiKeyTable {
    var result []*pb.TestMultiKeyTable
    for _, row := range m.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *TestMultiKeyTableManager) First(pred func(*pb.TestMultiKeyTable) bool) (*pb.TestMultiKeyTable, bool) {
    for _, row := range m.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

