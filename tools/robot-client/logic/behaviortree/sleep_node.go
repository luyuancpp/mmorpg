package behaviortree

import (
	b3 "github.com/magicsea/behavior3go"
	"time"

	. "github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
)

type SleepNode struct {
	Action
	Duration  time.Duration // 睡眠持续时间
	startTime time.Time     // 开始时间
	isRunning bool          // 是否正在运行
}

// Initialize 初始化节点的配置
func (s *SleepNode) Initialize(setting *BTNodeCfg) {
	s.Action.Initialize(setting)

	s.Duration = time.Duration(setting.GetPropertyAsInt64(`Duration`)) * time.Millisecond
}

// OnTick 执行节点逻辑
func (s *SleepNode) OnTick(tick *Tick) b3.Status {
	time.Sleep(s.Duration)
	return b3.SUCCESS
}
