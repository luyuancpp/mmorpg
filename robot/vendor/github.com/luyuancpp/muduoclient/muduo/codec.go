package muduo

import (
	"encoding/binary"
	"github.com/golang/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"hash/adler32"
	"log"
)

type Codec interface {
	Encode(message *proto.Message) ([]byte, error)
	Decode(data []byte) (proto.Message, uint32, error)
}

// TcpCodec 实现 muduo 的 Protobuf 编码格式
type TcpCodec struct {
	Codec
}

func GetDescriptor(message *proto.Message) protoreflect.MessageDescriptor {
	if message == nil {
		return nil
	}
	return proto.MessageReflect(*message).Descriptor()
}

func (c *TcpCodec) Encode(message *proto.Message) ([]byte, error) {
	desc := GetDescriptor(message)

	// TypeName 长度（包括一个额外的空格）
	typeName := desc.Name() + " "
	typeNameBytes := []byte(typeName)
	typeNameLen := len(typeNameBytes)

	// 消息体
	bodyBytes, err := proto.Marshal(*message)
	if err != nil {
		return nil, err
	}

	// 构建 msg 数据部分
	msgData := make([]byte, 0, 4+typeNameLen+len(bodyBytes)+4)
	nameLenBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(nameLenBuf, uint32(typeNameLen))
	msgData = append(msgData, nameLenBuf...)
	msgData = append(msgData, typeNameBytes...)
	msgData = append(msgData, bodyBytes...)

	// 校验和
	checksum := adler32.Checksum(msgData)
	checksumBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(checksumBuf, checksum)
	msgData = append(msgData, checksumBuf...)

	// 长度前缀
	totalLen := uint32(len(msgData))
	lengthBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(lengthBuf, totalLen)

	fullPacket := append(lengthBuf, msgData...)
	return fullPacket, nil
}

func (c *TcpCodec) Decode(data []byte) (proto.Message, uint32, error) {
	if len(data) < 8 {
		return nil, 0, nil
	}

	totalLen := binary.BigEndian.Uint32(data[0:4])
	if uint32(len(data)) < totalLen+4 {
		return nil, 0, nil
	}

	nameLen := binary.BigEndian.Uint32(data[4:8])
	typeNameEnd := 8 + nameLen
	if uint32(len(data)) < typeNameEnd {
		return nil, 0, nil
	}

	typeName := protoreflect.FullName(string(data[8 : typeNameEnd-1])) // 去除多余空格
	msgType, err := protoregistry.GlobalTypes.FindMessageByName(typeName)
	if err != nil {
		log.Println("Decode error: message type not found:", err)
		return nil, totalLen, err
	}

	msg := proto.MessageV1(msgType.New())
	bodyStart := typeNameEnd
	bodyEnd := totalLen
	err = proto.Unmarshal(data[bodyStart:bodyEnd], msg)
	if err != nil {
		return nil, totalLen, err
	}

	calculatedChecksum := adler32.Checksum(data[4:totalLen])
	expectedChecksum := binary.BigEndian.Uint32(data[totalLen : totalLen+4])
	if calculatedChecksum != expectedChecksum {
		log.Println("Checksum mismatch")
	}

	return msg, totalLen + 4, nil
} // RpcCodec 实现带固定标签 "RPC0" 的 Protobuf 编解码器
type RpcCodec struct {
	Codec
	RpcMsgType proto.Message
}

func (c *RpcCodec) Encode(message *proto.Message) ([]byte, error) {
	tag := []byte("RPC0")

	bodyBytes, err := proto.Marshal(*message)
	if err != nil {
		return nil, err
	}

	// 构建消息：tag + body
	payload := append(tag, bodyBytes...)

	// 校验和
	checksum := adler32.Checksum(payload)
	checksumBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(checksumBuf, checksum)

	// 加上头部长度
	packet := make([]byte, 0, 4+len(payload)+4)
	lengthBuf := make([]byte, 4)
	binary.BigEndian.PutUint32(lengthBuf, uint32(len(payload)+4)) // payload + checksum

	packet = append(packet, lengthBuf...)
	packet = append(packet, payload...)
	packet = append(packet, checksumBuf...)

	return packet, nil
}

func (c *RpcCodec) Decode(data []byte) (proto.Message, uint32, error) {
	if len(data) < 8 {
		return nil, 0, nil
	}

	packetLen := binary.BigEndian.Uint32(data[0:4])
	if uint32(len(data)) < packetLen+4 {
		return nil, 0, nil
	}

	fullName := protoreflect.FullName(GetDescriptor(&c.RpcMsgType).FullName())
	msgType, err := protoregistry.GlobalTypes.FindMessageByName(fullName)
	if err != nil {
		log.Println("Message type not found:", err)
		return nil, packetLen, err
	}

	msg := proto.MessageV1(msgType.New())
	err = proto.Unmarshal(data[8:packetLen], msg)
	if err != nil {
		return nil, packetLen, err
	}

	checksum := adler32.Checksum(data[4:packetLen])
	expectedChecksum := binary.BigEndian.Uint32(data[packetLen : packetLen+4])
	if checksum != expectedChecksum {
		log.Println("Checksum mismatch")
	}

	return msg, packetLen + 4, nil
}
