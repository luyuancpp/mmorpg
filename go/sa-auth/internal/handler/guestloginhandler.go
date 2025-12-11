package handler

import (
	"net/http"

	"github.com/zeromicro/go-zero/rest/httpx"
	"sa-auth/internal/logic"
	"sa-auth/internal/svc"
	"sa-auth/internal/types"
)

func GuestLoginHandler(svcCtx *svc.ServiceContext) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var req types.GuestLoginReq
		if err := httpx.Parse(r, &req); err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
			return
		}

		l := logic.NewGuestLoginLogic(r.Context(), svcCtx)
		resp, err := l.GuestLogin(&req)
		if err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
		} else {
			httpx.OkJsonCtx(r.Context(), w, resp)
		}
	}
}
