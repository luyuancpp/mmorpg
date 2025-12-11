package model

import (
	"github.com/zeromicro/go-zero/core/stores/cache"
	"github.com/zeromicro/go-zero/core/stores/sqlx"
)

var _ UserPasswordModel = (*customUserPasswordModel)(nil)

type (
	// UserPasswordModel is an interface to be customized, add more methods here,
	// and implement the added methods in customUserPasswordModel.
	UserPasswordModel interface {
		userPasswordModel
	}

	customUserPasswordModel struct {
		*defaultUserPasswordModel
	}
)

// NewUserPasswordModel returns a model for the database table.
func NewUserPasswordModel(conn sqlx.SqlConn, c cache.CacheConf, opts ...cache.Option) UserPasswordModel {
	return &customUserPasswordModel{
		defaultUserPasswordModel: newUserPasswordModel(conn, c, opts...),
	}
}
