package table

import (
    "fmt"
    "os"
    "path/filepath"

    "google.golang.org/protobuf/encoding/protojson"
    pb "your/proto/package/path"
)

type TestMultiKeyTableManager struct {
    data []*pb.TestMultiKey
    kvData map[int32]*pb.TestMultiKey
    kvStringkeyData map[string]*pb.TestMultiKey
    kvUint32keyData map[int32]*pb.TestMultiKey
    kvIn32keyData map[int32]*pb.TestMultiKey
    kvMstringkeyData map[string]*pb.TestMultiKey
    kvMuint32keyData map[int32]*pb.TestMultiKey
    kvMin32keyData map[int32]*pb.TestMultiKey
}

func NewTestMultiKeyTableManager() *TestMultiKeyTableManager {
    return &TestMultiKeyTableManager{
        kvData: make(map[int32]*pb.TestMultiKey),
        kvStringkeyData: make(map[string]*pb.TestMultiKey),
        kvUint32keyData: make(map[int32]*pb.TestMultiKey),
        kvIn32keyData: make(map[int32]*pb.TestMultiKey),
        kvMstringkeyData: make(map[string]*pb.TestMultiKey),
        kvMuint32keyData: make(map[int32]*pb.TestMultiKey),
        kvMin32keyData: make(map[int32]*pb.TestMultiKey),
    }
}

func (m *TestMultiKeyTableManager) Load(configDir string) error {
    path := filepath.Join(configDir, "testmultikey.json")
    raw, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("failed to read file: %%w", err)
    }

    var container pb.TestMultiKeyList
    if err := protojson.Unmarshal(raw, &container); err != nil {
        return fmt.Errorf("failed to parse json: %%w", err)
    }

    for _, row := range container.Data {
        m.kvData[row.Id] = row
        m.kvStringkeyData[row.Stringkey] = row
        m.kvUint32keyData[row.Uint32key] = row
        m.kvIn32keyData[row.In32key] = row
        m.kvMstringkeyData[row.Mstringkey] = row
        m.kvMuint32keyData[row.Muint32key] = row
        m.kvMin32keyData[row.Min32key] = row
    }

    m.data = container.Data
    return nil
}

func (m *TestMultiKeyTableManager) GetById(id int32) (*pb.TestMultiKey, bool) {
    row, ok := m.kvData[id]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByStringkey(key string) (*pb.TestMultiKey, bool) {
    row, ok := m.kvStringkeyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByUint32key(key int32) (*pb.TestMultiKey, bool) {
    row, ok := m.kvUint32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByIn32key(key int32) (*pb.TestMultiKey, bool) {
    row, ok := m.kvIn32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMstringkey(key string) (*pb.TestMultiKey, bool) {
    row, ok := m.kvMstringkeyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMuint32key(key int32) (*pb.TestMultiKey, bool) {
    row, ok := m.kvMuint32keyData[key]
    return row, ok
}
func (m *TestMultiKeyTableManager) GetByMin32key(key int32) (*pb.TestMultiKey, bool) {
    row, ok := m.kvMin32keyData[key]
    return row, ok
}