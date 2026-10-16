package svc

import (
	"context"
	"fmt"
	"os"
	"strings"
	"sync"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/service"

	"login/internal/config"
	"login/internal/logic/pkg/auth"
	"login/internal/logic/pkg/token"
)

// InitAuthProviders 按显式配置注册认证器。生产口令认证只读权威 MySQL 中的
// Argon2id 哈希；没有显式启用时仍 fail-closed。开发共享密钥认证与生产认证
// 互斥，避免配置合并时把弱入口带进生产。
func InitAuthProviders() (cleanup func()) {
	cleanup = func() {}
	initialized := false
	defer func() {
		if !initialized {
			cleanup()
		}
	}()
	if config.AppConfig.DevSkipAuth {
		panic("DevSkipAuth has been removed because it trusted arbitrary accounts; use the guarded DevPasswordAuth block")
	}

	productionPassword := config.AppConfig.PasswordAuth
	devPassword := config.AppConfig.DevPasswordAuth
	if productionPassword.Enabled && devPassword.Enabled {
		panic("PasswordAuth and DevPasswordAuth cannot both be enabled")
	}

	if productionPassword.Enabled {
		provider, closeProvider, err := productionPasswordProviderFromConfig(productionPassword, config.AppConfig.Node.RedisClient, os.LookupEnv)
		if err != nil {
			panic(fmt.Sprintf("initialize PasswordAuth provider: %v", err))
		}
		cleanup = closeProvider
		auth.RegisterPassword(provider)
		logx.Info("Production password auth registered (authoritative MySQL Argon2id, read-only)")
	}
	if devPassword.Enabled {
		if err := validateDevelopmentPasswordMode(config.AppConfig.Mode); err != nil {
			panic(fmt.Sprintf("invalid DevPasswordAuth configuration: %v", err))
		}
		provider, err := developmentPasswordProviderFromConfig(devPassword, os.LookupEnv)
		if err != nil {
			panic(fmt.Sprintf("invalid DevPasswordAuth configuration: %v", err))
		}
		auth.RegisterPassword(provider)
		logx.Errorf("SECURITY WARNING: development password auth ENABLED for account prefixes %v; never enable this in production",
			devPassword.AllowedAccountPrefixes)
	} else if !productionPassword.Enabled {
		logx.Info("Password auth disabled (fail-closed); configure PasswordAuth or guarded DevPasswordAuth")
	}

	cfg := config.AppConfig.AuthProviders

	if cfg.SaToken != nil {
		rdb := redis.NewClient(&redis.Options{
			Addr:             cfg.SaToken.Redis.Host,
			Password:         cfg.SaToken.Redis.Password,
			DB:               int(cfg.SaToken.Redis.DB),
			DialTimeout:      cfg.SaToken.Redis.DialTimeout,
			ReadTimeout:      cfg.SaToken.Redis.ReadTimeout,
			WriteTimeout:     cfg.SaToken.Redis.WriteTimeout,
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
			Endpoint:  cfg.WeChat.Endpoint,
		})
		if cfg.WeChat.Endpoint != "" {
			logx.Infof("Auth provider registered: wechat (endpoint override: %s)", cfg.WeChat.Endpoint)
		} else {
			logx.Info("Auth provider registered: wechat")
		}
	}

	if cfg.QQ != nil {
		auth.Register("qq", &auth.QQProvider{
			AppId:    cfg.QQ.AppId,
			AppKey:   cfg.QQ.AppKey,
			Endpoint: cfg.QQ.Endpoint,
		})
		if cfg.QQ.Endpoint != "" {
			logx.Infof("Auth provider registered: qq (endpoint override: %s)", cfg.QQ.Endpoint)
		} else {
			logx.Info("Auth provider registered: qq")
		}
	}

	if cfg.NetEase != nil {
		auth.Register("netease", &auth.NeteaseProvider{
			AppKey:    cfg.NetEase.AppKey,
			AppSecret: cfg.NetEase.AppSecret,
		})
		logx.Info("Auth provider registered: netease")
	}
	initialized = true
	return cleanup
}

// 专用小池使认证依赖具有独立硬上限；不继承主缓存池的重试/无界等待策略。
func passwordQuotaRedisOptions(conf config.RedisConf) *redis.Options {
	return &redis.Options{
		Addr: conf.Host, Password: conf.Password, DB: int(conf.DB),
		PoolSize: 4, MaxActiveConns: 4, MaxRetries: -1, DialerRetries: 1,
		ContextTimeoutEnabled: true, DisableIndentity: true,
		DialTimeout: auth.PasswordQuotaBudget, ReadTimeout: auth.PasswordQuotaBudget,
		WriteTimeout: auth.PasswordQuotaBudget, PoolTimeout: auth.PasswordQuotaBudget,
	}
}

func productionPasswordProviderFromConfig(conf config.PasswordAuthConf, redisConf config.RedisConf, lookup func(string) (string, bool)) (*auth.ProductionPasswordProvider, func(), error) {
	if err := conf.ValidateFailureQuota(); err != nil {
		return nil, nil, err
	}
	dsn, err := passwordAuthDSNFromConfig(conf, lookup)
	if err != nil {
		return nil, nil, err
	}
	store, err := auth.NewSQLPasswordHashStore(dsn, conf.MaxOpenConns, conf.MaxIdleConns)
	if err != nil {
		return nil, nil, err
	}
	rdb := redis.NewClient(passwordQuotaRedisOptions(redisConf))
	var once sync.Once
	cleanup := func() { once.Do(func() { _ = rdb.Close(); _ = store.Close() }) }
	ready := false
	defer func() {
		if !ready {
			cleanup()
		}
	}()
	quota, err := auth.NewRedisPasswordFailureQuota(rdb, conf.FailureLimit, conf.FailureWindow, conf.FailureLock)
	if err != nil {
		return nil, nil, err
	}
	ctx, cancel := context.WithTimeout(context.Background(), auth.PasswordQuotaBudget)
	defer cancel()
	if err := rdb.Ping(ctx).Err(); err != nil {
		return nil, nil, fmt.Errorf("password quota Redis startup unavailable")
	}
	provider, err := auth.NewProductionPasswordProviderWithLimits(store, quota, conf.KDFConcurrency, conf.KDFWaitTimeout)
	if err != nil {
		return nil, nil, err
	}
	ready = true
	return provider, cleanup, nil
}

func passwordAuthDSNFromConfig(conf config.PasswordAuthConf, lookup func(string) (string, bool)) (string, error) {
	envName := strings.TrimSpace(conf.DSNEnv)
	if envName == "" {
		return "", fmt.Errorf("DSNEnv must name a non-empty environment variable")
	}
	dsn, ok := lookup(envName)
	if !ok || strings.TrimSpace(dsn) == "" {
		return "", fmt.Errorf("environment variable %q is missing or empty", envName)
	}
	return dsn, nil
}

// validateDevelopmentPasswordMode 是 DevPasswordAuth 的启动门禁。只检查
// Enabled 不够：一份误带开关的生产配置即使有前缀和环境密钥，仍会把共享
// 密码认证暴露到生产。go-zero 的 dev/test Mode 是唯一允许注册该认证器的环境。
func validateDevelopmentPasswordMode(mode string) error {
	if mode == service.DevMode || mode == service.TestMode {
		return nil
	}
	return fmt.Errorf("DevPasswordAuth requires go-zero Mode %q or %q, got %q",
		service.DevMode, service.TestMode, mode)
}

// developmentPasswordProviderFromConfig 把“配置中的环境变量名”和真正密钥
// 分开。lookup 作为参数只用于单元测试；生产固定传 os.LookupEnv，禁止从 YAML
// 或其他受管配置字段直接读取共享密钥。
func developmentPasswordProviderFromConfig(conf config.DevPasswordAuthConf, lookup func(string) (string, bool)) (*auth.DevelopmentPasswordProvider, error) {
	envName := strings.TrimSpace(conf.SharedSecretEnv)
	if envName == "" {
		return nil, fmt.Errorf("SharedSecretEnv must name a non-empty environment variable")
	}
	sharedSecret, ok := lookup(envName)
	if !ok || sharedSecret == "" {
		return nil, fmt.Errorf("environment variable %q is missing or empty", envName)
	}
	return auth.NewDevelopmentPasswordProvider(sharedSecret, conf.AllowedAccountPrefixes)
}

// RegisterAccessTokenProvider registers the access_token provider.
// Called after TokenManager is created in NewServiceContext.
func RegisterAccessTokenProvider(tokenMgr *token.Manager) {
	auth.Register("access_token", &auth.AccessTokenProvider{
		TokenManager: tokenMgr,
	})
	logx.Info("Auth provider registered: access_token")
}
