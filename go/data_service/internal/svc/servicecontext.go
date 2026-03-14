package svc

import (
	"data_service/internal/config"
	"data_service/internal/routing"
)

type ServiceContext struct {
	Config config.Config
	Router *routing.Router
}

func NewServiceContext(c config.Config) *ServiceContext {
	return &ServiceContext{
		Config: c,
		Router: routing.NewRouter(c),
	}
}
