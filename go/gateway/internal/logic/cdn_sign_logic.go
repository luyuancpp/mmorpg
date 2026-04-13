package logic

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"time"

	"gateway/internal/svc"
	"gateway/internal/types"
)

const cdnSignTTLSeconds = 3600 // 1 hour

type CDNSignLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewCDNSignLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CDNSignLogic {
	return &CDNSignLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *CDNSignLogic) CDNSign(req *types.CDNSignRequest) (*types.CDNSignResponse, error) {
	expireAt := time.Now().Unix() + cdnSignTTLSeconds

	data := fmt.Sprintf("%s|%d", req.ResourcePath, expireAt)
	mac := hmac.New(sha256.New, []byte(l.svcCtx.Config.Gate.TokenSecret))
	mac.Write([]byte(data))
	sig := hex.EncodeToString(mac.Sum(nil))

	// TODO: replace with real CDN base URL from config
	signedURL := fmt.Sprintf("https://cdn.example.com%s?expire=%d&sign=%s", req.ResourcePath, expireAt, sig)

	return &types.CDNSignResponse{
		SignedURL: signedURL,
		ExpireAt:  expireAt,
	}, nil
}
