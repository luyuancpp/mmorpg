
package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "game/generated/pb/table"
)

type TestMultiKeyTableManager struct {
    data   []*pb.TestMultiKeyTable
    kvData map[uint32]*pb.TestMultiKeyTable
    kvString_keyData map[string]*pb.TestMultiKeyTable
    kvUint32_keyData map[uint32]*pb.TestMultiKeyTable
    kvInt32_keyData map[int32]*pb.TestMultiKeyTable
    kvM_string_keyData map[string]*pb.TestMultiKeyTable
    kvM_uint32_keyData map[uint32]*pb.TestMultiKeyTable
    kvM_int32_keyData map[int32]*pb.TestMultiKeyTable
    idxEffect map[uint32][]*pb.TestMultiKeyTable
}

var TestMultiKeyTableManagerInstance = NewTestMultiKeyTableManager()

func NewTestMultiKeyTableManager() *TestMultiKeyTableManager {
    return &TestMultiKeyTableManager{
        kvData: make(map[uint32]*pb.TestMultiKeyTable),
        kvString_keyData: make(map[string]*pb.TestMultiKeyTable),
        kvUint32_keyData: make(map[uint32]*pb.TestMultiKeyTable),
        kvInt32_keyData: make(map[int32]*pb.TestMultiKeyTable),
        kvM_string_keyData: make(map[string]*pb.TestMultiKeyTable),
        kvM_uint32_keyData: make(map[uint32]*pb.TestMultiKeyTable),
        kvM_int32_keyData: make(map[int32]*pb.TestMultiKeyTable),
        idxEffect: make(map[uint32][]*pb.TestMultiKeyTable),
    }
}

func (m *TestMultiKeyTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "testmultikey.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %w", err)
    }

    var container pb.TestMultiKeyTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        m.kvString_keyData[row.StringKey] = row
        m.kvUint32_keyData[row.Uint32Key] = row
        m.kvInt32_keyData[row.Int32Key] = row
        m.kvM_string_keyData[row.MStringKey] = row
        m.kvM_uint32_keyData[row.MUint32Key] = row
        m.kvM_int32_keyData[row.MInt32Key] = row
        for _, elem := range row.Effect {
            m.idxEffect[elem] = append(m.idxEffect[elem], row)
        }
    }

    m.data = container.Data
    return nil
}

func (m *TestMultiKeyTableManager) GetById(id uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByString_key(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvString_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByUint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvUint32_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByInt32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvInt32_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByM_string_key(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvM_string_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByM_uint32_key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvM_uint32_keyData[key]
    return row, ok
}

func (m *TestMultiKeyTableManager) GetByM_int32_key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvM_int32_keyData[key]
    return row, ok
}


func (m *TestMultiKeyTableManager) GetByEffectIndex(key uint32) []*pb.TestMultiKeyTable {
    return m.idxEffect[key]
}

