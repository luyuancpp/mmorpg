// Package token manages access/refresh token lifecycle in Redis.
//
// Token architecture (WeChat/QQ game login style):
//
//	Access Token:  opaque, 2h TTL, used for reconnect without re-auth from third-party.
//	Refresh Token: opaque, 30d TTL, used to obtain new access token after expiry.
//
// Redis keys:
//
//	access_token:{token}   → JSON{account, auth_type, device_id, created_at}   TTL=AccessTokenTTL
//	refresh_token:{token}  → JSON{account, auth_type, device_id, created_at}   TTL=RefreshTokenTTL
//	account_refresh:{account} → ZSET of active refresh tokens, score = refresh 过期 unix 秒
//	   (member 与其 refresh_token:{token} 键同生共死:键按 TTL 过期的同一刻,score 也越过
//	    now,可被 ZREMRANGEBYSCORE 一并清除。旧版是无 score 的 SET —— 成员不随对应键
//	    过期而消失,死成员无上界累积成 Redis 内存泄漏,客户端反复 Login 即可触发。)
package token

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	accessTokenPrefix  = "access_token:"
	refreshTokenPrefix = "refresh_token:"
	accountRefreshKey  = "account_refresh:"

	tokenByteLen = 32 // 256-bit random, base64url encoded → 43 chars

	// maxRefreshTokensPerAccount 封顶单账号活跃 refresh token 数。死成员由
	// ZREMRANGEBYSCORE 按过期 score 清,这个封顶再挡住"客户端高频 Login 在 30d
	// 窗口内把**活**成员刷到任意大"的病态情况:超出即淘汰最旧的,并删其 token 键。
	maxRefreshTokensPerAccount = 32
)

// Config holds token TTL settings.
type Config struct {
	AccessTokenTTL  time.Duration // e.g. 2h
	RefreshTokenTTL time.Duration // e.g. 30d (720h)
}

// TokenData is the payload stored in Redis for each token.
type TokenData struct {
	Account   string `json:"account"`
	AuthType  string `json:"auth_type"`
	DeviceID  string `json:"device_id,omitempty"`
	CreatedAt int64  `json:"created_at"`
}

// TokenPair is returned after issuing or refreshing tokens.
type TokenPair struct {
	AccessToken        string
	RefreshToken       string
	AccessTokenExpire  int64 // unix seconds
	RefreshTokenExpire int64 // unix seconds
}

// Manager handles token CRUD operations.
type Manager struct {
	rdb *redis.Client
	cfg Config
}

// NewManager creates a token manager.
func NewManager(rdb *redis.Client, cfg Config) *Manager {
	return &Manager{rdb: rdb, cfg: cfg}
}

// Issue generates a new access+refresh token pair and stores them in Redis.
func (m *Manager) Issue(ctx context.Context, account, authType, deviceID string) (*TokenPair, error) {
	now := time.Now()
	data := &TokenData{
		Account:   account,
		AuthType:  authType,
		DeviceID:  deviceID,
		CreatedAt: now.Unix(),
	}

	accessToken, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate access token: %w", err)
	}
	refreshToken, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate refresh token: %w", err)
	}

	dataBytes, err := json.Marshal(data)
	if err != nil {
		return nil, fmt.Errorf("marshal token data: %w", err)
	}

	accessExpire := now.Add(m.cfg.AccessTokenTTL)
	refreshExpire := now.Add(m.cfg.RefreshTokenTTL)

	setKey := accountRefreshKey + account
	pipe := m.rdb.TxPipeline()
	pipe.Set(ctx, accessTokenPrefix+accessToken, dataBytes, m.cfg.AccessTokenTTL)
	pipe.Set(ctx, refreshTokenPrefix+refreshToken, dataBytes, m.cfg.RefreshTokenTTL)
	// 先按 score 清掉已过期的死成员,再加入新成员。score = 该 refresh token 的
	// 过期 unix 秒;score < now 的成员其 refresh_token 键早已 TTL 过期,清成员即可,
	// 无需再删键。这一步把"死成员无上界累积"根治掉。
	pipe.ZRemRangeByScore(ctx, setKey, "-inf", fmt.Sprintf("%d", now.Unix()-1))
	pipe.ZAdd(ctx, setKey, redis.Z{Score: float64(refreshExpire.Unix()), Member: refreshToken})
	pipe.Expire(ctx, setKey, m.cfg.RefreshTokenTTL)
	if _, err := pipe.Exec(ctx); err != nil {
		return nil, fmt.Errorf("store tokens in Redis: %w", err)
	}

	// 封顶活跃成员数(best-effort,失败不阻断签发):超出即淘汰最旧的若干枚,
	// 并连带删除它们的 refresh_token 键,让集合与 token 键都保持有界。
	m.trimRefreshSet(ctx, setKey)

	logx.Infof("[Token] Issued tokens for account=%s auth_type=%s access_ttl=%v refresh_ttl=%v",
		account, authType, m.cfg.AccessTokenTTL, m.cfg.RefreshTokenTTL)

	return &TokenPair{
		AccessToken:        accessToken,
		RefreshToken:       refreshToken,
		AccessTokenExpire:  accessExpire.Unix(),
		RefreshTokenExpire: refreshExpire.Unix(),
	}, nil
}

// ValidateAccess validates an access token, returns the account if valid.
func (m *Manager) ValidateAccess(ctx context.Context, accessToken string) (*TokenData, error) {
	return m.validate(ctx, accessTokenPrefix+accessToken)
}

// rotateRefreshScript 把「作废旧 refresh token」与「签发新的一对」并成**一次**原子提交。
//
// 为什么不再分步(旧版 DEL → ZREM → Issue 的 MULTI 是三次往返):请求在三步之间截止时
// (zrpc 服务端 Timeout,docs/design/client-request-ordering.md §6.1),go-redis 不再发后续命令,
// 留下「旧的已作废、新的没签出」—— 客户端手里的 refresh token 作废,只能回去走第三方重新认证;
// 连接在两步之间断开也是同样结局。合成一段 Lua 之后提交点只有一个:截止前没发出就什么都没变,
// 发出了就整体生效。
//
// 旧 token 按**值**比对(compare-and-delete),而不是只看 DEL 的返回值:Refresh 先 GET 出旧 token 的
// payload 拿到账号(集合键要由它拼出,必须经 KEYS 传入),再交给脚本。两步之间被并发的另一次刷新消费
// 或已过期,脚本就读不到同一份值,整段不写。go-redis 对读超时的 EVALSHA 原样重发时也一样:第二次执行
// 看到旧 token 已不在,回 0、不会二次签发。
//
// KEYS = {旧 refresh 键, account_refresh 集合, 新 access 键, 新 refresh 键}
// ARGV = {旧 refresh 键当前的值, 旧 refresh token(集合成员), 新 payload, access TTL(毫秒),
//         refresh TTL(毫秒), 死成员清理的 score 上界, 新成员 score, 新 refresh token(集合成员)}
// 写入顺序与 Issue 的 MULTI 相同(先清死成员再加新成员,见 Issue 注释)。
// 返回 1 = 已轮换;0 = 旧 token 已不是读到的那一份,什么都没写。
var rotateRefreshScript = redis.NewScript(`
if redis.call("GET", KEYS[1]) ~= ARGV[1] then
    return 0
end
redis.call("DEL", KEYS[1])
redis.call("ZREM", KEYS[2], ARGV[2])
redis.call("SET", KEYS[3], ARGV[3], "PX", ARGV[4])
redis.call("SET", KEYS[4], ARGV[3], "PX", ARGV[5])
redis.call("ZREMRANGEBYSCORE", KEYS[2], "-inf", ARGV[6])
redis.call("ZADD", KEYS[2], ARGV[7], ARGV[8])
redis.call("PEXPIRE", KEYS[2], ARGV[5])
return 1
`)

// rotateRefreshScript 的返回值。
const (
	refreshRotated  = 1
	refreshConsumed = 0
)

// Refresh validates a refresh token, invalidates it, and issues a new token pair (rotation).
//
// 作废与签发是同一次原子提交(rotateRefreshScript):要么旧的仍有效、新的没签,要么旧的作废、新的已签。
// 提交前先查 ctx:请求已截止 / 已取消就不发脚本(截止之后 gate 已放行同一会话的下一条请求,
// 见 client-request-ordering.md §6.1)。已知残留(不在本函数解决):脚本已在 Redis 执行、回包却丢了时,
// 新的一对签出了但客户端拿不到,旧的已作废,重试必然失败(同文 §7.2 第 5 条,是重试问题,不是顺序问题)。
func (m *Manager) Refresh(ctx context.Context, refreshToken string) (*TokenPair, error) {
	// 非正 TTL 会让 SET PX / PEXPIRE 报错或签出永不过期的 token,提交前显式拒绝(fail-closed)。
	if m.cfg.AccessTokenTTL <= 0 || m.cfg.RefreshTokenTTL <= 0 {
		return nil, fmt.Errorf("token TTL must be positive (access=%v refresh=%v)", m.cfg.AccessTokenTTL, m.cfg.RefreshTokenTTL)
	}
	key := refreshTokenPrefix + refreshToken
	raw, data, err := m.load(ctx, key)
	if err != nil {
		return nil, err
	}

	now := time.Now()
	newAccess, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate access token: %w", err)
	}
	newRefresh, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate refresh token: %w", err)
	}
	payload, err := json.Marshal(&TokenData{
		Account:   data.Account,
		AuthType:  data.AuthType,
		DeviceID:  data.DeviceID,
		CreatedAt: now.Unix(),
	})
	if err != nil {
		return nil, fmt.Errorf("marshal token data: %w", err)
	}
	accessExpire := now.Add(m.cfg.AccessTokenTTL)
	refreshExpire := now.Add(m.cfg.RefreshTokenTTL)
	setKey := accountRefreshKey + data.Account

	if err := ctx.Err(); err != nil {
		return nil, fmt.Errorf("request done before rotating refresh token, nothing written: %w", err)
	}
	res, err := rotateRefreshScript.Run(ctx, m.rdb,
		[]string{key, setKey, accessTokenPrefix + newAccess, refreshTokenPrefix + newRefresh},
		raw, refreshToken, payload,
		m.cfg.AccessTokenTTL.Milliseconds(), m.cfg.RefreshTokenTTL.Milliseconds(),
		now.Unix()-1, refreshExpire.Unix(), newRefresh,
	).Int64()
	if err != nil {
		return nil, fmt.Errorf("rotate refresh token: %w", err)
	}
	switch res {
	case refreshRotated:
	case refreshConsumed:
		// 并发的另一次刷新先消费了它,或在 GET 与脚本之间过期。
		return nil, fmt.Errorf("refresh token already consumed")
	default:
		return nil, fmt.Errorf("unexpected rotateRefreshScript result %d", res)
	}

	// 提交之后的封顶清理,与 Issue 相同是 best-effort:请求恰好此刻截止时这一次不清,集合最多暂时多出
	// 一枚活成员,下一次签发会清掉。
	m.trimRefreshSet(ctx, setKey)

	logx.Infof("[Token] Rotated refresh token for account=%s auth_type=%s access_ttl=%v refresh_ttl=%v",
		data.Account, data.AuthType, m.cfg.AccessTokenTTL, m.cfg.RefreshTokenTTL)

	return &TokenPair{
		AccessToken:        newAccess,
		RefreshToken:       newRefresh,
		AccessTokenExpire:  accessExpire.Unix(),
		RefreshTokenExpire: refreshExpire.Unix(),
	}, nil
}

// trimRefreshSet 把 account_refresh ZSET 封顶到 maxRefreshTokensPerAccount 枚活跃成员:
// 超出则淘汰 score 最小(最旧)的若干枚,并删除它们的 refresh_token 键。best-effort。
func (m *Manager) trimRefreshSet(ctx context.Context, setKey string) {
	n, err := m.rdb.ZCard(ctx, setKey).Result()
	if err != nil || n <= maxRefreshTokensPerAccount {
		return
	}
	// 最旧的 (n - max) 枚:ZSET 按 score 升序,rank [0, n-max-1] 即最旧那批。
	overflow, err := m.rdb.ZRange(ctx, setKey, 0, n-maxRefreshTokensPerAccount-1).Result()
	if err != nil || len(overflow) == 0 {
		return
	}
	delKeys := make([]string, 0, len(overflow))
	members := make([]interface{}, 0, len(overflow))
	for _, t := range overflow {
		delKeys = append(delKeys, refreshTokenPrefix+t)
		members = append(members, t)
	}
	pipe := m.rdb.TxPipeline()
	pipe.Del(ctx, delKeys...)
	pipe.ZRem(ctx, setKey, members...)
	if _, err := pipe.Exec(ctx); err != nil {
		logx.Errorf("[Token] trim refresh set %s failed: %v", setKey, err)
	}
}

// RevokeAll revokes all refresh tokens for an account (e.g. password change).
func (m *Manager) RevokeAll(ctx context.Context, account string) error {
	setKey := accountRefreshKey + account
	tokens, err := m.rdb.ZRange(ctx, setKey, 0, -1).Result()
	if err != nil {
		return fmt.Errorf("list refresh tokens: %w", err)
	}

	if len(tokens) > 0 {
		keys := make([]string, 0, len(tokens)+1)
		for _, t := range tokens {
			keys = append(keys, refreshTokenPrefix+t)
		}
		keys = append(keys, setKey)
		m.rdb.Del(ctx, keys...)
	}

	logx.Infof("[Token] Revoked all refresh tokens for account=%s count=%d", account, len(tokens))
	return nil
}

func (m *Manager) validate(ctx context.Context, key string) (*TokenData, error) {
	_, data, err := m.load(ctx, key)
	return data, err
}

// load 读出 token 键的原始字节并解析。Refresh 要原始字节交给轮换脚本做按值比对(见 rotateRefreshScript)。
func (m *Manager) load(ctx context.Context, key string) ([]byte, *TokenData, error) {
	val, err := m.rdb.Get(ctx, key).Bytes()
	if err == redis.Nil {
		return nil, nil, fmt.Errorf("token not found or expired")
	}
	if err != nil {
		return nil, nil, fmt.Errorf("Redis lookup: %w", err)
	}

	var data TokenData
	if err := json.Unmarshal(val, &data); err != nil {
		return nil, nil, fmt.Errorf("unmarshal token data: %w", err)
	}
	return val, &data, nil
}

func generateToken() (string, error) {
	b := make([]byte, tokenByteLen)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return base64.RawURLEncoding.EncodeToString(b), nil
}
