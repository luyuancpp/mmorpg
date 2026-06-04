package config

import (
	"fmt"
	"time"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/zrpc"
)

// Config is the top-level configuration.
type Config struct {
	zrpc.RpcServerConf                    // Embedded go-zero RPC config (ListenOn, Etcd, etc.)
	Node               NodeConfig         `json:"Node"`
	Snowflake          SnowflakeConf      `json:"Snowflake"`
	Locker             LockerConf         `json:"Locker"`
	Account            AccountConf        `json:"Account"`
	Registry           RegistryConf       `json:"Registry"`
	Timeouts           TimeoutConf        `json:"Timeouts"`
	Kafka              KafkaConfig        `json:"Kafka"`
	PlayerLocatorRpc   zrpc.RpcClientConf `json:"PlayerLocatorRpc"`   // player_locator gRPC client
	SceneManagerRpc    zrpc.RpcClientConf `json:"SceneManagerRpc"`    // scene_manager gRPC client
	GateTokenSecret    string             `json:"GateTokenSecret"`    // HMAC secret for gate connection tokens
	TableDir           string             `json:",default=../../generated/tables"`
	AuthProviders      AuthConfig         `json:"AuthProviders,optional"` // Third-party auth provider config
	DevSkipAuth        bool               `json:"DevSkipAuth,optional"` // Dev mode: skip auth provider validation, use account field directly
	TokenConfig        TokenConf          `json:"TokenConfig,optional"` // Access/refresh token TTL settings
	PreloadPool        PreloadPoolConf    `json:"PreloadPool,optional"` // Background goroutine pool for player data preload

	// LegacyGateLoginEnabled controls whether the deprecated path
	// "Client → cpp gate (TCP) → ClientPlayerLogin.Login" is still served.
	//
	// Background: ARCH §12 T+2 step. The new path (Java Gateway POST
	// /api/login → gRPC login) has been the recommended route since
	// 2026-05; the legacy path is kept as backwards compatibility with
	// throttled deprecation telemetry (legacy_login_count counter).
	// Once that counter trends below the agreed threshold per T+1 exit
	// criteria, ops flips this flag to false and the legacy branch
	// returns kLoginUnknownError, forcing the few remaining old
	// clients to fall through to /api/login (or their own retry path,
	// which typically re-resolves via /api/assign-gate).
	//
	// Detection is the same SessionDetails-based heuristic
	// loginlogic.go uses for telemetry: a non-zero SessionId in the
	// gRPC context means the call arrived through cpp gate's
	// HandleGrpcNodeMessage forwarder (legacy). When false here, that
	// branch short-circuits BEFORE acquiring locks or writing
	// player_locator state, so flipping the flag back is purely a
	// config change with no cleanup required.
	//
	// Default true preserves current behavior. Operators flip via
	// login.yaml; no restart-blocking dependency on other services.
	LegacyGateLoginEnabled bool `json:"LegacyGateLoginEnabled,default=true"`

	// Queue holds configuration for the AssignGate login queue. Disabled by
	// default so existing deployments keep the legacy "always assign" behavior;
	// flip Enabled=true once the queue path has been validated end-to-end. See
	// docs/design/login-queue-2026-05.md (added with this feature).
	Queue QueueConf `json:"Queue,optional"`
}

// QueueConf controls the login queue (Redis ZSET-backed) that throttles
// AssignGate when a zone's gates are at capacity.
//
// Knobs are deliberately conservative:
//   - DispatchInterval too low burns Redis QPS for no benefit (gate
//     PlayerCount only refreshes every few seconds via etcd anyway).
//   - AdmitTTL must outlive the worst-case "client received admit token →
//     reconnects to /assign-gate → connects to gate" round-trip; 60s covers
//     mobile-network jitter and short backoffs comfortably.
//   - SoftCapMultiplier (e.g. 1.5) is the headroom factor applied to the
//     largest observed PlayerCount when no explicit ZoneCapacityOverride is
//     set, so freshly-deployed zones aren't capped at "current load".
type QueueConf struct {
	Enabled              bool          `json:"Enabled,default=false"`
	DispatchInterval     time.Duration `json:"DispatchInterval,default=1s"`
	AdmitTTL             time.Duration `json:"AdmitTTL,default=60s"`
	QueueEntryTTL        time.Duration `json:"QueueEntryTTL,default=1h"`
	SoftCapMultiplier    float64       `json:"SoftCapMultiplier,default=1.5"`
	DefaultRetryAfterMs  uint32        `json:"DefaultRetryAfterMs,default=2000"`
	DispatcherLockTTL    time.Duration `json:"DispatcherLockTTL,default=30s"`
	DispatcherLockKey    string        `json:"DispatcherLockKey,default=dispatcher:lock:login_queue"`
	// ZoneCapacityOverride lets ops pin the per-zone admission ceiling
	// without trusting gate-side soft caps. Key is zone_id (string for YAML),
	// value is the absolute number of concurrent online players permitted.
	ZoneCapacityOverride map[string]uint32 `json:"ZoneCapacityOverride,optional"`
}

// PreloadPoolConf controls the bounded goroutine pool used to fan out
// fire-and-forget player data preload tasks (Kafka -> DB -> Redis warm).
// Defaults are tuned for ~256 concurrent in-flight preloads, which is far
// more than the Kafka SyncProducer can drain (it is mutex-serialized), so
// the pool acts as a bounded queue + backpressure / overload shield.
type PreloadPoolConf struct {
	Size           int           `json:"Size,default=256"`           // Worker count
	StatsInterval  time.Duration `json:"StatsInterval,default=30s"` // Periodic snapshot log interval (0 = disabled)
}

// AuthConfig holds third-party auth provider settings.
type AuthConfig struct {
	SaToken  *SaTokenAuthConf  `json:"SaToken,optional"`
	WeChat   *WeChatAuthConf   `json:"WeChat,optional"`
	QQ       *QQAuthConf       `json:"QQ,optional"`
	NetEase  *NeteaseAuthConf  `json:"NetEase,optional"`
}

// SaTokenAuthConf holds SA-Token Redis lookup settings.
type SaTokenAuthConf struct {
	Redis     RedisConf `json:"Redis"`
	TokenName string    `json:"TokenName,default=satoken"`
	LoginType string    `json:"LoginType,default=login"`
}

// WeChatAuthConf holds WeChat OAuth settings.
type WeChatAuthConf struct {
	AppId     string `json:"AppId"`
	AppSecret string `json:"AppSecret"`
	// Endpoint optionally overrides the api.weixin.qq.com base URL.
	// Set to a local sandbox mock (e.g. http://127.0.0.1:18090) when
	// real Open Platform credentials / network egress aren't available.
	// Production leaves this empty so the provider hits the real host.
	Endpoint string `json:"Endpoint,optional"`
}

// QQAuthConf holds QQ OAuth settings.
type QQAuthConf struct {
	AppId  string `json:"AppId"`
	AppKey string `json:"AppKey"`
	// Endpoint optionally overrides the graph.qq.com base URL.
	// Same semantics as WeChatAuthConf.Endpoint.
	Endpoint string `json:"Endpoint,optional"`
}

// NeteaseAuthConf holds NetEase auth settings.
type NeteaseAuthConf struct {
	AppKey    string `json:"AppKey"`
	AppSecret string `json:"AppSecret"`
}

// TokenConf holds access/refresh token TTL settings.
type TokenConf struct {
	AccessTokenTTL  time.Duration `json:"AccessTokenTTL,default=2h"`  // Access token lifetime (default 2 hours)
	RefreshTokenTTL time.Duration `json:"RefreshTokenTTL,default=720h"` // Refresh token lifetime (default 30 days)
}

// NodeConfig holds node-level settings including login duration limits.
type NodeConfig struct {
	ZoneId           uint32        `json:"ZoneId"`
	SessionExpireMin uint32        `json:"SessionExpireMin"` // Session idle timeout in minutes
	MaxLoginDevices  uint32        `json:"MaxLoginDevices"`
	RedisClient      RedisConf     `json:"RedisClient"`
	LeaseTTL         int64         `json:"LeaseTTL"` // Lease TTL in seconds
	QueueShardCount  uint64        `json:"QueueShardCount"`
	MaxLoginDuration time.Duration `json:"MaxLoginDuration"` // Max online duration per login (e.g. 24h, force logout on expiry)
	LogoutGraceTime  time.Duration `json:"LogoutGraceTime"`  // Grace period before forced logout (e.g. 5m, for player warning)
}

// RedisConf holds Redis client settings.
type RedisConf struct {
	Host         string        `json:"Host"`
	Password     string        `json:"Password"`
	DB           uint32        `json:"DB"`
	DefaultTTL   time.Duration `json:"DefaultTTL"`
	DialTimeout  time.Duration `json:"DialTimeout"`
	ReadTimeout  time.Duration `json:"ReadTimeout"`
	WriteTimeout time.Duration `json:"WriteTimeout"`
}

// KafkaConfig holds Kafka producer and consumer settings.
type KafkaConfig struct {
	Brokers          []string                `json:"Brokers"`
	GroupID          string                  `json:"GroupID"`
	Topic            string                  `json:"Topic,optional"` // Derived from Node.ZoneId at startup
	PartitionCnt     int32                   `json:"PartitionCnt"`
	InitialPartition int                     `json:"InitialPartition"` // Should match PartitionCnt
	DialTimeout      time.Duration           `json:"DialTimeout"`
	ReadTimeout      time.Duration           `json:"ReadTimeout"`
	WriteTimeout     time.Duration           `json:"WriteTimeout"`
	RetryMax         int                     `json:"RetryMax"`
	RetryBackoff     time.Duration           `json:"RetryBackoff"`
	ChannelBuffer    int                     `json:"ChannelBuffer"`
	SyncInterval     time.Duration           `json:"SyncInterval"`
	StatsInterval    time.Duration           `json:"StatsInterval"`
	CompressionType  sarama.CompressionCodec `json:"CompressionType"` // none/gzip/snappy
	Idempotent       bool                    `json:"Idempotent"`
	MaxOpenRequests  int                     `json:"MaxOpenRequests"` // Must be 1 when idempotent
	RetentionMs      int64                   `json:"RetentionMs,default=86400000"` // Topic retention in ms (default 24h)
	                                                                                  // P1 数据安全加固 2026-06-03: 旧值 300000 (5min)
	                                                                                  // 太短,db service 卡住会丢数据
}

// SnowflakeConf holds snowflake ID generator settings.
type SnowflakeConf struct {
	Epoch    int64  `json:"Epoch"` // Epoch timestamp in milliseconds
	NodeBits uint32 `json:"NodeBits"`
	StepBits uint32 `json:"StepBits"`
}

// LockerConf holds distributed lock settings.
type LockerConf struct {
	AccountLockTTL uint32 `json:"AccountLockTTL"` // Account lock TTL in seconds
	PlayerLockTTL  uint32 `json:"PlayerLockTTL"`  // Player lock TTL in seconds
}

// AccountConf holds account-related settings.
type AccountConf struct {
	MaxDevicesPerAccount int64         `json:"MaxDevicesPerAccount"`
	MaxPlayersPerAccount int           `json:"MaxPlayersPerAccount,default=5"`
	CacheExpire          time.Duration `json:"CacheExpire"`
}

// RegistryConf holds service registry/discovery settings.
type RegistryConf struct {
	Etcd EtcdRegistryConf `json:"Etcd"`
}

// EtcdRegistryConf holds etcd registry settings.
type EtcdRegistryConf struct {
	Hosts       []string      `json:"Hosts"`
	Key         string        `json:"Key"`
	DialTimeout time.Duration `json:"DialTimeout"`
}

// TimeoutConf holds various timeout settings.
type TimeoutConf struct {
	EtcdDialTimeout         time.Duration `json:"EtcdDialTimeout"`
	ServiceDiscoveryTimeout time.Duration `json:"ServiceDiscoveryTimeout"`
	TaskWaitTimeout         time.Duration `json:"TaskWaitTimeout"`
	LoginTotalTimeout       time.Duration `json:"LoginTotalTimeout"` // Total login timeout including Redis/DB
	RoleCacheExpire         time.Duration `json:"RoleCacheExpire"`
}

var AppConfig Config

// DbTaskTopic returns the zone-specific Kafka topic for DB tasks.
func DbTaskTopic(zoneId uint32) string {
	return fmt.Sprintf("db_task_zone_%d", zoneId)
}
