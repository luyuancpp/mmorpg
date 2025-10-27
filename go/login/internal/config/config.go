package config

import (
	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/zrpc"
	"time"
)

// Config 总配置结构体
type Config struct {
	zrpc.RpcServerConf               // 继承zrpc的基础配置（ListenOn、Etcd等）
	Node               NodeConfig    `json:"Node"`      // 节点配置（新增登录时长限制）
	Snowflake          SnowflakeConf `json:"Snowflake"` // 雪花ID配置
	Locker             LockerConf    `json:"Locker"`    // 锁配置
	Account            AccountConf   `json:"Account"`   // 账号配置
	Registry           RegistryConf  `json:"Registry"`  // 服务注册发现配置
	Timeouts           TimeoutConf   `json:"Timeouts"`  // 各类超时配置
	Kafka              KafkaConfig   `json:"Kafka"`     // Kafka配置
}

// NodeConfig 节点相关配置（新增登录时长限制）
type NodeConfig struct {
	ZoneId           uint32    `json:"ZoneId"`           // 区服ID
	SessionExpireMin uint32    `json:"SessionExpireMin"` // 会话过期时间（分钟，如会话闲置超时）
	MaxLoginDevices  uint32    `json:"MaxLoginDevices"`  // 最大登录设备数
	RedisClient      RedisConf `json:"RedisClient"`      // Redis客户端配置
	LeaseTTL         int64     `json:"LeaseTTL"`         // 租约超时时间（秒）
	QueueShardCount  uint64    `json:"QueueShardCount"`  // 队列分片数
	// 新增：登录时长限制（单次登录后保持在线的最大时长）
	MaxLoginDuration time.Duration `json:"MaxLoginDuration"` // 最大登录时长（如24h，超时强制下线）
	LogoutGraceTime  time.Duration `json:"LogoutGraceTime"`  // 超时后 grace 时间（如5m，用于提醒玩家）
}

// RedisConf Redis客户端配置
type RedisConf struct {
	Host         string        `json:"Host"`         // 地址（如127.0.0.1:6379）
	Password     string        `json:"Password"`     // 密码
	DB           uint32        `json:"DB"`           // 数据库编号
	PoolSize     uint32        `json:"PoolSize"`     // 连接池大小
	DefaultTTL   time.Duration `json:"DefaultTTL"`   // 默认过期时间（如24h）
	DialTimeout  time.Duration `json:"DialTimeout"`  // 连接超时（如3s）
	ReadTimeout  time.Duration `json:"ReadTimeout"`  // 读取超时（如3s）
	WriteTimeout time.Duration `json:"WriteTimeout"` // 写入超时（如3s）
}

// KafkaConfig Kafka配置（含生产者和消费者）
type KafkaConfig struct {
	BootstrapServers string                  `json:"Brokers"`          // 集群地址（逗号分隔，如127.0.0.1:9092）
	GroupID          string                  `json:"GroupID"`          // 消费者组ID
	Topic            string                  `json:"Topic"`            // 主题名
	PartitionCnt     int32                   `json:"PartitionCnt"`     // 分区数量
	InitialPartition int                     `json:"InitialPartition"` // 初始分区数（建议与PartitionCnt一致）
	DialTimeout      time.Duration           `json:"DialTimeout"`      // 拨号超时（如10s）
	ReadTimeout      time.Duration           `json:"ReadTimeout"`      // 读取超时（如30s）
	WriteTimeout     time.Duration           `json:"WriteTimeout"`     // 写入超时（如10s）
	RetryMax         int                     `json:"RetryMax"`         // 最大重试次数
	RetryBackoff     time.Duration           `json:"RetryBackoff"`     // 重试间隔（如100ms）
	ChannelBuffer    int                     `json:"ChannelBuffer"`    // 通道缓冲大小
	SyncInterval     time.Duration           `json:"SyncInterval"`     // 分区同步间隔（如30s）
	StatsInterval    time.Duration           `json:"StatsInterval"`    // 统计间隔（如5m）
	CompressionType  sarama.CompressionCodec `json:"CompressionType"`  // 压缩类型（none/gzip/snappy）
	Idempotent       bool                    `json:"Idempotent"`       // 是否启用幂等性
	MaxOpenRequests  int                     `json:"MaxOpenRequests"`  // 最大并发请求数（幂等性需设为1）
}

// SnowflakeConf 雪花ID生成器配置
type SnowflakeConf struct {
	Epoch    int64  `json:"Epoch"`    // 起始时间戳（毫秒，如1721473263000）
	NodeBits uint32 `json:"NodeBits"` // 节点ID位数
	StepBits uint32 `json:"StepBits"` // 序列号位数
}

// LockerConf 分布式锁配置
type LockerConf struct {
	AccountLockTTL uint32 `json:"AccountLockTTL"` // 账号锁超时（秒）
	PlayerLockTTL  uint32 `json:"PlayerLockTTL"`  // 角色锁超时（秒）
}

// AccountConf 账号相关配置
type AccountConf struct {
	MaxDevicesPerAccount int64         `json:"MaxDevicesPerAccount"` // 单账号最大设备数
	CacheExpire          time.Duration `json:"CacheExpire"`          // 账号缓存过期时间（如12h）
}

// RegistryConf 服务注册发现配置
type RegistryConf struct {
	Etcd EtcdRegistryConf `json:"Etcd"` // Etcd配置
}

// EtcdRegistryConf Etcd注册中心配置
type EtcdRegistryConf struct {
	Hosts       []string      `json:"Hosts"`       // 地址列表（如["127.0.0.1:2379"]）
	Key         string        `json:"Key"`         // 服务注册键名
	DialTimeout time.Duration `json:"DialTimeout"` // 连接超时（如5s）
}

// TimeoutConf 各类超时配置
type TimeoutConf struct {
	EtcdDialTimeout          time.Duration `json:"EtcdDialTimeout"`          // Etcd连接超时（如5s）
	ServiceDiscoveryTimeout  time.Duration `json:"ServiceDiscoveryTimeout"`  // 服务发现超时（如10s）
	TaskWaitTimeout          time.Duration `json:"TaskWaitTimeout"`          // 任务等待超时（如5s）
	LoginTotalTimeout        time.Duration `json:"LoginTotalTimeout"`        // 登录过程总超时（含Redis/DB，如10s）
	RoleCacheExpire          time.Duration `json:"RoleCacheExpire"`          // 角色缓存过期时间（如24h）
	TaskManagerCleanInterval time.Duration `json:"TaskManagerCleanInterval"` // 任务批次清理间隔
	TaskBatchExpireTime      time.Duration `json:"TaskBatchExpireTime"`      // 任务批次过期时间
}

// 全局配置实例
var AppConfig Config
