package handler

import (
	"net/http"

	"github.com/zeromicro/go-zero/rest/httpx"
	"sa-auth/internal/logic"
	"sa-auth/internal/svc"
	"sa-auth/internal/types"
)

func PasswordLoginHandler(svcCtx *svc.ServiceContext) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var req types.PasswordLoginReq
		if err := httpx.Parse(r, &req); err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
			return
		}

		l := logic.NewPasswordLoginLogic(r.Context(), svcCtx)
		resp, err := l.PasswordLogin(&req)
		if err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
		} else {
			httpx.OkJsonCtx(r.Context(), w, resp)
		}
	}
}
