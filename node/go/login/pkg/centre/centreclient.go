package centre

import (
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"github.com/zeromicro/go-zero/core/logx"
	"login/pb/game"
)

type CentreClient struct {
	CentreClient *muduo.Client
}

func NewCentreClient(ip string, port int) *CentreClient {
	muduoClient, _ := muduo.NewClient(ip, port)
	return &CentreClient{
		CentreClient: muduoClient,
	}
}

func (c *CentreClient) Close() error {
	return c.CentreClient.Close()
}

func (c *CentreClient) Send(m proto.Message, messageId uint32) {
	var err error
	rpcMsg := &game.RpcMessage{}
	rpcMsg.MessageId = messageId
	rpcMsg.Request, err = proto.Marshal(m)
	if err != nil {
		logx.Error(err)
		return
	}
	c.CentreClient.Send(rpcMsg)
}
