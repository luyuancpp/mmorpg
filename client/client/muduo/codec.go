package muduo

import (
	"encoding/binary"
	"github.com/golang/protobuf/proto"
	"hash/adler32"
)

func Encode(m *proto.Message) ([]byte, error) {
	//from zinx
	d := GetDescriptor(m)

	/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
	///
	/// @code
	/// +-------------------+------------------+------------------+
	/// | prependable bytes |  readable bytes  |  writable bytes  |
	/// |                   |     (CONTENT)    |                  |
	/// +-------------------+------------------+------------------+
	/// |                   |                  |                  |
	/// 0      <=      readerIndex   <=   writerIndex    <=     size

	pbNameLenData := make([]byte, 4)
	pbTypeName := d.Name() + " "
	binary.BigEndian.PutUint32(pbNameLenData, uint32(len(pbTypeName)))
	pbTypeNameData := []byte(pbTypeName)

	pbBodyData, err := proto.Marshal(*m)
	if err != nil {
		return []byte{}, err
	}

	dataPB := make([]byte, 0)

	dataPB = append(dataPB, pbNameLenData...)
	dataPB = append(dataPB, pbTypeNameData...)
	dataPB = append(dataPB, pbBodyData...)

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
