package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "db/generated/pb/table"
)

var TestMultiKeyTableManagerInstance = NewTestMultiKeyTableManager()


type TestMultiKeyTableManager struct {
    data []*pb.TestMultiKeyTable
    kvData map[uint32]*pb.TestMultiKeyTable
    kvStringkeyData map[string]*pb.TestMultiKeyTable
    kvUint32keyData map[uint32]*pb.TestMultiKeyTable
    kvIn32keyData map[int32]*pb.TestMultiKeyTable
    kvMstringkeyData map[string]*pb.TestMultiKeyTable
    kvMuint32keyData map[uint32]*pb.TestMultiKeyTable
    kvMin32keyData map[int32]*pb.TestMultiKeyTable
}

func NewTestMultiKeyTableManager() *TestMultiKeyTableManager {
    return &TestMultiKeyTableManager{
        kvData: make(map[uint32]*pb.TestMultiKeyTable),
        kvStringkeyData: make(map[string]*pb.TestMultiKeyTable),
        kvUint32keyData: make(map[uint32]*pb.TestMultiKeyTable),
        kvIn32keyData: make(map[int32]*pb.TestMultiKeyTable),
        kvMstringkeyData: make(map[string]*pb.TestMultiKeyTable),
        kvMuint32keyData: make(map[uint32]*pb.TestMultiKeyTable),
        kvMin32keyData: make(map[int32]*pb.TestMultiKeyTable),
    }
}

func (m *TestMultiKeyTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "testmultikey.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.TestMultiKeyTableData
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        m.kvStringkeyData[row.Stringkey] = row
        m.kvUint32keyData[row.Uint32Key] = row
        m.kvIn32keyData[row.In32Key] = row
        m.kvMstringkeyData[row.Mstringkey] = row
        m.kvMuint32keyData[row.Muint32Key] = row
        m.kvMin32keyData[row.Min32Key] = row
    }

    m.data = container.Data
    return nil
}

func (m *TestMultiKeyTableManager) GetById(id uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvData[id]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByStringkey(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvStringkeyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByUint32key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvUint32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByIn32key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvIn32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMstringkey(key string) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvMstringkeyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMuint32key(key uint32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvMuint32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMin32key(key int32) (*pb.TestMultiKeyTable, bool) {
    row, ok := m.kvMin32keyData[key]
    return row, ok
}