package svc

import (
	"data_service/internal/config"
	"data_service/internal/routing"
	"data_service/internal/store"

	"github.com/zeromicro/go-zero/core/logx"
)

type ServiceContext struct {
	Config        config.Config
	Router        *routing.Router
	SnapshotStore *store.SnapshotStore
}

func NewServiceContext(c config.Config) *ServiceContext {
	ss, err := store.NewSnapshotStore(store.MySQLConfig{
		Host:        c.SnapshotMySQL.Host,
		User:        c.SnapshotMySQL.User,
		Password:    c.SnapshotMySQL.Password,
		DBName:      c.SnapshotMySQL.DBName,
		MaxOpenConn: c.SnapshotMySQL.MaxOpenConn,
		MaxIdleConn: c.SnapshotMySQL.MaxIdleConn,
	})
	if err != nil {
		logx.Errorf("[ServiceContext] snapshot store init failed (rollback disabled): %v", err)
	}

	return &ServiceContext{
		Config:        c,
		Router:        routing.NewRouter(c),
		SnapshotStore: ss,
	}
}
