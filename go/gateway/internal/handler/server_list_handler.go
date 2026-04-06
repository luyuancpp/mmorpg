package handler

import (
	"net/http"

	"gateway/internal/logic"
	"gateway/internal/svc"

	"github.com/zeromicro/go-zero/rest/httpx"
)

func ServerListHandler(svcCtx *svc.ServiceContext) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		l := logic.NewServerListLogic(r.Context(), svcCtx)
		resp, err := l.ServerList()
		if err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
			return
		}
		httpx.OkJsonCtx(r.Context(), w, resp)
	}
}
