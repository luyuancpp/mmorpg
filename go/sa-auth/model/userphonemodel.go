package model

import (
	"github.com/zeromicro/go-zero/core/stores/cache"
	"github.com/zeromicro/go-zero/core/stores/sqlx"
)

var _ UserPhoneModel = (*customUserPhoneModel)(nil)

type (
	// UserPhoneModel is an interface to be customized, add more methods here,
	// and implement the added methods in customUserPhoneModel.
	UserPhoneModel interface {
		userPhoneModel
	}

	customUserPhoneModel struct {
		*defaultUserPhoneModel
	}
)

// NewUserPhoneModel returns a model for the database table.
func NewUserPhoneModel(conn sqlx.SqlConn, c cache.CacheConf, opts ...cache.Option) UserPhoneModel {
	return &customUserPhoneModel{
		defaultUserPhoneModel: newUserPhoneModel(conn, c, opts...),
	}
}
