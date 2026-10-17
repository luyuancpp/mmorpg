//go:build !windows

package domainhost

import (
	"github.com/zeromicro/go-zero/core/proc"
	"github.com/zeromicro/go-zero/zrpc"
	"time"
)

// 留给NodeInfo注销和主动排空；最终硬截止仍由go-zero持有。
func configureShutdown(c *zrpc.RpcServerConf) {
	c.Shutdown.WrapUpTime = 24 * time.Second
	c.Shutdown.WaitTime = 24 * time.Second
	proc.Setup(c.Shutdown)
}
