package svc

import (
	"context"
	"testing"

	"friend/internal/config"
	base "proto/common/base"
	"shared/loginsession"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/require"
)

func TestLoginSessionClientUsesOnlyExplicitGlobalDatabase(t *testing.T) {
	server := miniredis.RunT(t)
	server.Select(0)
	require.NoError(t, server.Set("login_session:gate:8", "wrong-db"))
	server.Select(3)
	require.NoError(t, server.Set("login_session:gate:8", "original"))
	c := config.Config{GlobalLoginSessions: true, LoginRedis: config.LoginRedisConf{Host: server.Addr(), DB: 3, PoolSize: 8}}
	client, err := NewLoginSessionClient(c)
	require.NoError(t, err)
	require.Equal(t, 8, client.Options().MaxActiveConns)
	t.Cleanup(func() { _ = client.Close() })
	account, err := loginsession.Account(context.Background(), client, &base.SessionDetails{SessionId: 1, GateInstanceId: "gate", SessionIncarnation: 8})
	require.NoError(t, err)
	require.Equal(t, "original", account)
	c.GlobalLoginSessions = false
	_, err = NewLoginSessionClient(c)
	require.Error(t, err, "不允许回落到共享缓存Redis")
}
