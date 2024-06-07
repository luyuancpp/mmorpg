package muduo

import (
	"bytes"
	"github.com/golang/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"log"
	"net"
	"sync"
	"sync/atomic"
)

type Connection struct {
	conn         net.Conn
	OutputBuffer bytes.Buffer
	NeedClose    atomic.Bool
	mu           sync.Mutex
	M            chan proto.Message
}

func GetDescriptor(m *proto.Message) protoreflect.MessageDescriptor {
	reflection := proto.MessageReflect(*m)
	return reflection.Descriptor()
}

func (c *Connection) HandleWriteMsg() {
	for {
		m := <-c.M
		data, err := Encode(&m)
		if err != nil {
			log.Println(err)
			return
		}
		c.WriteToBuffer(data)
	}
}

func (c *Connection) WriteToBuffer(data []byte) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.OutputBuffer.Write(data)
}

func (c *Connection) SendToConn() {
	if c.OutputBuffer.Len() <= 0 {
		return
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	_, err := c.OutputBuffer.WriteTo(c.conn)
	if err != nil {
		log.Println(err)
		return
	}
}

func (c *Connection) HandleWrite() {
	for {
		if c.NeedClose.Load() {
			return
		}
		c.SendToConn()
	}
}
