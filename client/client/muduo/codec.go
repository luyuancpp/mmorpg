package muduo

import (
	"encoding/binary"
	"github.com/golang/protobuf/proto"
	"hash/adler32"
)

func Encode(m *proto.Message) ([]byte, error) {
	//from zinx

	d := GetDescriptor(m)

	nameLen := make([]byte, 4)
	name := d.Name() + " "
	binary.BigEndian.PutUint32(nameLen, uint32(len(name)))

	typeName := []byte(name)

	body, err := proto.Marshal(*m)
	if err != nil {
		return []byte{}, err
	}

	dataPB := make([]byte, 0)

	dataPB = append(dataPB, nameLen...)
	dataPB = append(dataPB, typeName...)
	dataPB = append(dataPB, body...)

	checkSum := adler32.Checksum(dataPB)
	checkSumData := make([]byte, 4)
	binary.BigEndian.PutUint32(checkSumData, checkSum)
	dataPB = append(dataPB, checkSumData...)

	lenPB := len(dataPB)
	lenData := make([]byte, 4)
	binary.BigEndian.PutUint32(lenData, uint32(lenPB))
	data := make([]byte, 0)
	data = append(data, lenData...)
	data = append(data, dataPB...)

	return data, nil
}
