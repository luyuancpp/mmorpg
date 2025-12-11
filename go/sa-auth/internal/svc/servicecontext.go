package svc

import (
	"github.com/zeromicro/go-zero/core/stores/sqlx"
	"sa-auth/internal/config"
	"sa-auth/model"
)

type ServiceContext struct {
	Config            config.Config
	UserModel         model.UserModel
	UserOauthModel    model.UserOauthModel
	UserPasswordModel model.UserPasswordModel
	UserPhoneModel    model.UserPhoneModel
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config:            c,
		UserModel:         model.NewUserModel(sqlx.NewMysql(c.Mysql.DataSource), c.Cache),
		UserOauthModel:    model.NewUserOauthModel(sqlx.NewMysql(c.Mysql.DataSource), c.Cache),
		UserPasswordModel: model.NewUserPasswordModel(sqlx.NewMysql(c.Mysql.DataSource), c.Cache),
		UserPhoneModel:    model.NewUserPhoneModel(sqlx.NewMysql(c.Mysql.DataSource), c.Cache),
	}
}
