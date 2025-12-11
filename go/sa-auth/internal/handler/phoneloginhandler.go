package handler

import (
	"net/http"

	"github.com/zeromicro/go-zero/rest/httpx"
	"sa-auth/internal/logic"
	"sa-auth/internal/svc"
	"sa-auth/internal/types"
)

func PhoneLoginHandler(svcCtx *svc.ServiceContext) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var req types.PhoneLoginReq
		if err := httpx.Parse(r, &req); err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
			return
		}

		l := logic.NewPhoneLoginLogic(r.Context(), svcCtx)
		resp, err := l.PhoneLogin(&req)
		if err != nil {
			httpx.ErrorCtx(r.Context(), w, err)
		} else {
			httpx.OkJsonCtx(r.Context(), w, resp)
		}
	}
}
