package centre

import (
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"github.com/zeromicro/go-zero/core/logx"
	"login/pb/game"
)

type Client struct {
	CentreClient *muduo.Client
}

func NewCentreClient(ip string, port uint32) *Client {
	client, _ := muduo.NewClient(ip, int(port), &muduo.RpcCodec{RpcMsgType: &game.GameRpcMessage{}})
	return &Client{
		CentreClient: client,
	}
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
