package centre

import (
	"fmt"
	"github.com/google/uuid"
	"github.com/luyuancpp/muduoclient-new/muduo"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"login/generated/pb/game"
)

type Client struct {
	CentreClient *muduo.TcpClient
	NodeUuid     uuid.UUID
}

func NewCentreClient(ip string, port uint32, uuidStr string) (*Client, error) {
	parsedUUID, err := uuid.Parse(uuidStr)
	if err != nil {
		return nil, fmt.Errorf("invalid UUID string: %w", err)
	}
	addr := fmt.Sprintf("%s:%d", ip, port)
	client := muduo.NewTcpClient(addr, &muduo.RpcCodec{MsgType: &game.GameRpcMessage{}})

	return &Client{
		CentreClient: client,
		NodeUuid:     parsedUUID,
	}, nil
}

func (c *Client) Close() {
	c.CentreClient.Close()
}

func (c *Client) Send(m proto.Message, messageId uint32) {
	var err error
	rpcMsg := &game.GameRpcMessage{}
	rpcMsg.MessageId = messageId
	rpcMsg.Request, err = proto.Marshal(m)
	if err != nil {
		logx.Error(err)
		return
	}
	c.CentreClient.Send(rpcMsg)
}
