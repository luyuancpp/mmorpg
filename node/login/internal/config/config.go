package config

import (
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/zrpc"
)

type Config struct {
	zrpc.RpcServerConf
	Node NodeConfig `json:"node"`
}

type NodeConfig struct {
	ZoneId           uint32 `json:"zoneid,optional"`
	SessionExpireMin int    `json:"session_expire_minutes"`
	FsmExpireMin     int    `json:"fsm_expire_minutes"`
	MaxLoginDevices  int    `json:"max_login_devices"`
}

// 用于存储配置数据的全局变量
var AppConfig Config // 现在使用 AppConfig 来存储配置数据

// init 函数会在程序启动时自动执行
func init() {
	conf.MustLoad("etc/loginservice.yaml", &AppConfig)
}
