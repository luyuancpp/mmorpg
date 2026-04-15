
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



// testmultikeySnapshot holds all parsed data and indices.
// Load() builds a new snapshot and swaps it in, replacing the old one.
type testmultikeySnapshot struct {
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

type TestMultiKeyTableManager struct {
    snap *testmultikeySnapshot
}

var TestMultiKeyTableManagerInstance = NewTestMultiKeyTableManager()

func NewTestMultiKeyTableManager() *TestMultiKeyTableManager {
    return &TestMultiKeyTableManager{
        snap: &testmultikeySnapshot{
            kvData: make(map[uint32]*pb.TestMultiKeyTable),
            kvString_keyData: make(map[string]*pb.TestMultiKeyTable),
            kvUint32_keyData: make(map[uint32]*pb.TestMultiKeyTable),
            kvInt32_keyData: make(map[int32]*pb.TestMultiKeyTable),
            kvM_string_keyData: make(map[string][]*pb.TestMultiKeyTable),
            kvM_uint32_keyData: make(map[uint32][]*pb.TestMultiKeyTable),
            kvM_int32_keyData: make(map[int32][]*pb.TestMultiKeyTable),
            idxEffect: make(map[uint32][]*pb.TestMultiKeyTable),
        },
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

    snap := &testmultikeySnapshot{
        kvData: make(map[uint32]*pb.TestMultiKeyTable, len(container.Data)),
        kvString_keyData: make(map[string]*pb.TestMultiKeyTable, len(container.Data)),
        kvUint32_keyData: make(map[uint32]*pb.TestMultiKeyTable, len(container.Data)),
        kvInt32_keyData: make(map[int32]*pb.TestMultiKeyTable, len(container.Data)),
        kvM_string_keyData: make(map[string][]*pb.TestMultiKeyTable),
        kvM_uint32_keyData: make(map[uint32][]*pb.TestMultiKeyTable),
        kvM_int32_keyData: make(map[int32][]*pb.TestMultiKeyTable),
        idxEffect: make(map[uint32][]*pb.TestMultiKeyTable),
    }

    for _, row := range container.Data {
        snap.kvData[row.Id] = row
        snap.kvString_keyData[row.StringKey] = row
        snap.kvUint32_keyData[row.Uint32Key] = row
        snap.kvInt32_keyData[row.Int32Key] = row
        snap.kvM_string_keyData[row.MStringKey] = append(snap.kvM_string_keyData[row.MStringKey], row)
        snap.kvM_uint32_keyData[row.MUint32Key] = append(snap.kvM_uint32_keyData[row.MUint32Key], row)
        snap.kvM_int32_keyData[row.MInt32Key] = append(snap.kvM_int32_keyData[row.MInt32Key], row)
        for _, elem := range row.Effect {
            snap.idxEffect[elem] = append(snap.idxEffect[elem], row)
        }
    }

    snap.data = container.Data
    m.snap = snap
    return nil
}

func (m *TestMultiKeyTableManager) FindAll() []*pb.TestMultiKeyTable {
    return m.snap.data
}

func (m *TestMultiKeyTableManager) FindById(id uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.snap.kvData[id]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByString_key(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.snap.kvString_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByUint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.snap.kvUint32_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) FindByInt32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.snap.kvInt32_keyData[key]
    return row, ok
}


func (m *TestMultiKeyTableManager) FindByM_string_key(key string) []*pb.TestMultiKeyTable {
    return m.snap.kvM_string_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByM_uint32_key(key uint32) []*pb.TestMultiKeyTable {
    return m.snap.kvM_uint32_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByM_int32_key(key int32) []*pb.TestMultiKeyTable {
    return m.snap.kvM_int32_keyData[key]
}


func (m *TestMultiKeyTableManager) FindByEffectIndex(key uint32) []*pb.TestMultiKeyTable {
    return m.snap.idxEffect[key]
}



// ---- Exists ----

func (m *TestMultiKeyTableManager) Exists(id uint32) bool {
    _, ok := m.snap.kvData[id]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByString_key(key string) bool {
    _, ok := m.snap.kvString_keyData[key]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByUint32_key(key uint32) bool {
    _, ok := m.snap.kvUint32_keyData[key]
    return ok
}

func (m *TestMultiKeyTableManager) ExistsByInt32_key(key int32) bool {
    _, ok := m.snap.kvInt32_keyData[key]
    return ok
}



// ---- Count ----

func (m *TestMultiKeyTableManager) Count() int {
    return len(m.snap.data)
}

func (m *TestMultiKeyTableManager) CountByM_string_key(key string) int {
    return len(m.snap.kvM_string_keyData[key])
}

func (m *TestMultiKeyTableManager) CountByM_uint32_key(key uint32) int {
    return len(m.snap.kvM_uint32_keyData[key])
}

func (m *TestMultiKeyTableManager) CountByM_int32_key(key int32) int {
    return len(m.snap.kvM_int32_keyData[key])
}


func (m *TestMultiKeyTableManager) CountByEffectIndex(key uint32) int {
    return len(m.snap.idxEffect[key])
}



// ---- FindByIds (IN) ----

func (m *TestMultiKeyTableManager) FindByIds(ids []uint32) []*pb.TestMultiKeyTable {
    result := make([]*pb.TestMultiKeyTable, 0, len(ids))
    for _, id := range ids {
        if row, ok := m.snap.kvData[id]; ok {
            result = append(result, row)
        }
    }
    return result
}

// ---- RandOne ----

func (m *TestMultiKeyTableManager) RandOne() (*pb.TestMultiKeyTable, bool) {
    if len(m.snap.data) == 0 {
        return nil, false
    }
    return m.snap.data[rand.IntN(len(m.snap.data))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_string_key(key string) (*pb.TestMultiKeyTable, bool) {
    rows := m.snap.kvM_string_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_uint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    rows := m.snap.kvM_uint32_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}

func (m *TestMultiKeyTableManager) RandOneByM_int32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    rows := m.snap.kvM_int32_keyData[key]
    if len(rows) == 0 {
        return nil, false
    }
    return rows[rand.IntN(len(rows))], true
}



// ---- Where / First ----

func (m *TestMultiKeyTableManager) Where(pred func(*pb.TestMultiKeyTable) bool) []*pb.TestMultiKeyTable {
    var result []*pb.TestMultiKeyTable
    for _, row := range m.snap.data {
        if pred(row) {
            result = append(result, row)
        }
    }
    return result
}

func (m *TestMultiKeyTableManager) First(pred func(*pb.TestMultiKeyTable) bool) (*pb.TestMultiKeyTable, bool) {
    for _, row := range m.snap.data {
        if pred(row) {
            return row, true
        }
    }
    return nil, false
}

// ---- Composite Key ----

