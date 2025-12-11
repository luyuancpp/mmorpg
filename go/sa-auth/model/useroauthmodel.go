package model

import (
	"github.com/zeromicro/go-zero/core/stores/cache"
	"github.com/zeromicro/go-zero/core/stores/sqlx"
)

var _ UserOauthModel = (*customUserOauthModel)(nil)

type (
	// UserOauthModel is an interface to be customized, add more methods here,
	// and implement the added methods in customUserOauthModel.
	UserOauthModel interface {
		userOauthModel
	}

	customUserOauthModel struct {
		*defaultUserOauthModel
	}
)

// NewUserOauthModel returns a model for the database table.
func NewUserOauthModel(conn sqlx.SqlConn, c cache.CacheConf, opts ...cache.Option) UserOauthModel {
	return &customUserOauthModel{
		defaultUserOauthModel: newUserOauthModel(conn, c, opts...),
	}
}
