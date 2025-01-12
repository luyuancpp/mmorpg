package data

import (
	fsm "github.com/looplab/fsm"
	"login/pb/game"
)

// Session 定义玩家结构体
type Session struct {
	Account     string
	Fsm         *fsm.FSM
	UserAccount *game.UserAccounts
}

// 为什么要有状态，因为要处理可能我在登录的任意阶段发各种协议，比如加载数据过程中发一次进入游戏
// 这时候服务器数据流不能出错，应该告诉客户端请稍等，正在登录中
// 学楚留香手游，一个账号支持多个游戏角色在线

// NewPlayer 创建一个新的 Session 对象并初始化状态机
func NewPlayer(account string) *Session {
	player := &Session{
		Account: account,
	}

	// 定义事件
	player.Fsm = InitPlayerFSM()

	return player
}
