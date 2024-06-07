package muduo

import (
	"github.com/golang/protobuf/proto"
)

import (
	"net"
	"strconv"
)

type Client struct {
	Conn Connection
}

func NewClient(ip string, port int) (*Client, error) {
	conn, err := net.Dial("tcp", ip+":"+strconv.Itoa(port))
	if err != nil {
		panic(err)
		return nil, err
	}

	c := &Client{
		Conn: Connection{conn: conn, M: make(chan proto.Message, 20)},
	}

	go c.Conn.HandleWriteMsg()
	go c.Conn.HandleWrite()
	return c, nil
}

func (c *Client) Close() error {
	return c.Conn.conn.Close()
}

func (c *Client) Send(m proto.Message) {
	c.Conn.M <- m
}
