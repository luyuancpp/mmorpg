package svc

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"

	"login/internal/config"
	"login/internal/logic/pkg/auth"
)

// InitAuthProviders registers auth providers based on the Auth config section.
// "password" provider is always registered. Others are registered only when configured.
func InitAuthProviders() {
	auth.Register("password", &auth.PasswordProvider{})
	logx.Info("Auth provider registered: password")

	cfg := config.AppConfig.Auth

	if cfg.SaToken != nil {
		rdb := redis.NewClient(&redis.Options{
			Addr:             cfg.SaToken.Redis.Host,
			Password:         cfg.SaToken.Redis.Password,
			DB:               int(cfg.SaToken.Redis.DB),
			DisableIndentity: true,
		})
		if err := rdb.Ping(context.Background()).Err(); err != nil {
			panic(fmt.Errorf("failed to connect SA-Token Redis for auth provider: %w", err))
		}
		auth.Register("satoken", &auth.SaTokenProvider{
			RedisClient: rdb,
			TokenName:   cfg.SaToken.TokenName,
			LoginType:   cfg.SaToken.LoginType,
		})
		logx.Infof("Auth provider registered: satoken (Redis=%s)", cfg.SaToken.Redis.Host)
	}

	if cfg.WeChat != nil {
		auth.Register("wechat", &auth.WeChatProvider{
			AppId:     cfg.WeChat.AppId,
			AppSecret: cfg.WeChat.AppSecret,
		})
		logx.Info("Auth provider registered: wechat")
	}

	if cfg.QQ != nil {
		auth.Register("qq", &auth.QQProvider{
			AppId:  cfg.QQ.AppId,
			AppKey: cfg.QQ.AppKey,
		})
		logx.Info("Auth provider registered: qq")
	}

	if cfg.NetEase != nil {
		auth.Register("netease", &auth.NeteaseProvider{
			AppKey:    cfg.NetEase.AppKey,
			AppSecret: cfg.NetEase.AppSecret,
		})
		logx.Info("Auth provider registered: netease")
	}
}
