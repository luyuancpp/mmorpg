package handler

import (
	"gateway/internal/svc"

	"github.com/zeromicro/go-zero/rest"
)

// RegisterRoutes registers all HTTP API routes on the go-zero rest server.
func RegisterRoutes(server *rest.Server, svcCtx *svc.ServiceContext) {
	server.AddRoutes([]rest.Route{
		{
			Method:  "GET",
			Path:    "/api/server-list",
			Handler: ServerListHandler(svcCtx),
		},
		{
			Method:  "POST",
			Path:    "/api/assign-gate",
			Handler: AssignGateHandler(svcCtx),
		},
		{
			Method:  "GET",
			Path:    "/api/announcement",
			Handler: AnnouncementHandler(svcCtx),
		},
		{
			Method:  "POST",
			Path:    "/api/hotfix-check",
			Handler: HotfixCheckHandler(svcCtx),
		},
		{
			Method:  "POST",
			Path:    "/api/cdn-sign",
			Handler: CDNSignHandler(svcCtx),
		},
	})
}
