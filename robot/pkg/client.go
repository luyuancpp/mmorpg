package pkg

import (
	"client/logic/behaviortree"
	"client/pb/game"
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	b3 "github.com/magicsea/behavior3go"
	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	"go.uber.org/zap"
)

type GameClient struct {
	Client       *muduo.Client
	PlayerId     uint64
	BehaviorTree []*BehaviorTree
	Blackboard   *Blackboard
}

func NewGameClient(client *muduo.Client) *GameClient {

	// 加载行为树配置文件
	projectConfig, result := LoadRawProjectCfg("etc/robot.b3")
	if !result {
		zap.L().Error("Failed to load behavior tree configuration")
		return nil
	}

	// 自定义节点注册
	maps := b3.NewRegisterStructMaps()
	maps.Register("SendCreatePlayer", new(behaviortree.SendCreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.IsRoleListEmpty)) // 确保你的行为树中需要该节点

	// 初始化行为树
	behaviorTree := make([]*BehaviorTree, len(projectConfig.Data.Trees))
	for i, v := range projectConfig.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		behaviorTree[i] = tree
	}

	return &GameClient{Client: client, BehaviorTree: behaviorTree, Blackboard: NewBlackboard()}
}

func (c *GameClient) Send(message proto.Message, messageId uint32) {
	rq := &game.ClientRequest{Id: 1, MessageId: messageId}
	var err error
	rq.Body, err = proto.Marshal(message)
	if err != nil {
		zap.L().Error("send message ", zap.Error(err))
		return
	}
	c.Client.Send(rq)
}

func (c *GameClient) Close() {
	defer func(client *muduo.Client) {
		err := client.Close()
		if err != nil {
			panic(err)
		}
	}(c.Client)
}

// TickBehaviorTree 更新所有行为树的状态
func (c *GameClient) TickBehaviorTree() {
	for i, tree := range c.BehaviorTree {
		tree.Tick(i, c.Blackboard)
	}
}
