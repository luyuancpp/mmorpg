package handler

import (
	"net/http"

	"github.com/zeromicro/go-zero/rest/httpx"
	"sa-auth/internal/logic"
	"sa-auth/internal/svc"
	"sa-auth/internal/types"
)

func OAuthLoginHandler(svcCtx *svc.ServiceContext) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var req types.OAuthLoginReq
		if err := httpx.Parse(r, &req); err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
			return
		}

		l := logic.NewOAuthLoginLogic(r.Context(), svcCtx)
		resp, err := l.OAuthLogin(&req)
		if err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
		} else {
			httpx.OkJsonCtx(r.Context(), w, resp)
		}
	}
}
