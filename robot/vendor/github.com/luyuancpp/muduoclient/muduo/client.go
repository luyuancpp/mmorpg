package muduo

import (
	"github.com/golang/protobuf/proto"
)

import (
	"net"
	"strconv"
)

type Client struct {
	Conn *Connection
}

func NewClient(ip string, port int, codec Codec) (*Client, error) {
	addr := net.JoinHostPort(ip, strconv.Itoa(port))
	conn := NewConnection(addr, codec)

	c := &Client{
		Conn: conn,
	}

	return c, nil
}

func (c *Client) Close() {
	c.Conn.Close()
}

func (c *Client) Send(m proto.Message) {
	err := c.Conn.Send(m)
	if err != nil {
		return
	}
}

func (c *Client) Recv() (proto.Message, error) {
	return c.Conn.Recv()
}
