package svc

import (
	"data_service/internal/config"
	"data_service/internal/routing"
	"data_service/internal/store"

	loginpb "data_service/proto/login"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
)

type ServiceContext struct {
	Config           config.Config
	Router           *routing.Router
	SnapshotStore    *store.SnapshotStore
	TxLogStore       *store.TransactionLogStore
	LoginAdminClient loginpb.LoginAdminClient // nil when not configured
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

	txLog, err := store.NewTransactionLogStore(store.MySQLConfig{
		Host:        c.SnapshotMySQL.Host,
		User:        c.SnapshotMySQL.User,
		Password:    c.SnapshotMySQL.Password,
		DBName:      c.SnapshotMySQL.DBName,
		MaxOpenConn: c.SnapshotMySQL.MaxOpenConn,
		MaxIdleConn: c.SnapshotMySQL.MaxIdleConn,
	})
	if err != nil {
		logx.Errorf("[ServiceContext] transaction log store init failed (recall/query disabled): %v", err)
	}

	// Optional: login admin gRPC client for orphan account cleanup during rollback
	var loginClient loginpb.LoginAdminClient
	if c.LoginAdminRpc.Etcd.Key != "" {
		conn := zrpc.MustNewClient(c.LoginAdminRpc)
		loginClient = loginpb.NewLoginAdminClient(conn.Conn())
		logx.Info("[ServiceContext] login admin RPC client connected")
	}

	return &ServiceContext{
		Config:           c,
		Router:           routing.NewRouter(c),
		SnapshotStore:    ss,
		TxLogStore:       txLog,
		LoginAdminClient: loginClient,
	}
}
