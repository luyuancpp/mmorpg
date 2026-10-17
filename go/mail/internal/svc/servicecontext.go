// Package svc 是 mail 服务的依赖装配:etcd 客户端、mmorpg_mail 连接池、共享 Redis(只读会话)、
// data_service 客户端与 mail_id 号段、S2C 推送用的 Kafka writer。
//
// 与 friend 一样,本包**不含** Prometheus 指标(指标在 internal/metrics,懒注册,由 main 调用)。
package svc

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"sync"
	"time"

	"mail/internal/config"
	"mail/internal/data"
	mailkafka "mail/internal/kafka"

	dspb "proto/data_service"

	"shared/idsegment"
	"shared/kafkacmd"
	"shared/kafkautil"

	// database/sql 只认注册过的驱动名;这个空导入就是注册 "mysql" 的唯一动作,删了它
	// sql.Open("mysql", ...) 会在运行期报 unknown driver,而编译期一声不响。
	_ "github.com/go-sql-driver/mysql"
	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"github.com/zeromicro/go-zero/zrpc"
	clientv3 "go.etcd.io/etcd/client/v3"
)

const (
	// etcdDialTimeout / mysqlPingTimeout 与 friend / trade 同值:探库必须有上限,MySQL 不可达时 TCP 连接
	// 可能挂到内核超时(分钟级),K8s 的 startupProbe 会先判失败重启 Pod,日志里看不到"库连不上"这句因果。
	etcdDialTimeout  = 5 * time.Second
	mysqlPingTimeout = 5 * time.Second

	// kafkaWriteTimeout 与 friend / match 同口径:kafka-go Writer 零值 WriteTimeout 是 10s,比整请求预算还长,
	// 一次 broker 抖动就能把发件的响应拖成调用方超时(推送发生在回包之前,见 M1c 的 push)。
	kafkaWriteTimeout = 5 * time.Second

	// mailIDWarmTimeout:启动时同步领第一段号的上限;领不到只告警(之后每次 Next 会自己再试)。
	mailIDWarmTimeout = 5 * time.Second

	// MailIDBizTag 是 mail_id / sys_mail_id 共用的号段 tag(设计 §3.7)。必须已在 data_service 的 BootstrapTags 里
	// (go/data_service 两处 DefaultIdSegmentBootstrapTags、etc/data_service.yaml、k8s ConfigMap),否则生产发号被拒。
	MailIDBizTag = "mail"
)

// ServiceContext 持有全部依赖。构造后只读;Stop 只生效一次。
type ServiceContext struct {
	Config config.Config

	// Etcd 供 noderegistry 注册与 killswitch 监听共用(Etcd.Key 为空,go-zero 不写自己的 key,契约 D-13)。
	Etcd *clientv3.Client

	// DB 是 mmorpg_mail 的连接池(D-14)。表由 schemamigrate 建,本包一行 DDL 都不发。
	DB *sql.DB

	// SharedRedis 是跨运行时契约 key player:session:{id} 的**唯一句柄**,mail 只读它(推送前找收件人所在 gate)。
	// node 形态,config.Validate 拒 Cluster。go-zero 的 redis.Redis 没有 Close,不由业务关。
	SharedRedis *redis.Redis
	// Sessions 是建在 SharedRedis 上的会话读取器。
	Sessions *data.SessionReader

	// DataService 是领号段用的 data_service 客户端;dataServiceConn 是它的连接,Stop 时关。
	DataService     dspb.DataServiceClient
	dataServiceConn interface{ Close() error }

	// MailIDs 是 mail_id / sys_mail_id 号段客户端(biz_tag=mail)。没有 snowflake 回退:领不到号 = 发件回 kServiceUnavailable。
	MailIDs *idsegment.Client

	// KafkaWriter 是 S2C 推送的出口。**没配 broker 时是 nil**:推送是 at-most-once 的旁路,缺它不该拒启,
	// 但调用方必须自己判空(nil Writer 上调 WriteMessages 会 panic)。
	KafkaWriter *kafkago.Writer

	// GateCommandBuilder 把消息号 + 序列化体包成 GateCommand。它用的是本服务自己的 mail/generated/pb/game 常量,
	// 所以只能留在 mail 里、不能上提到 shared(friend 同理)。
	GateCommandBuilder kafkautil.GateCommandBuilder

	stopOnce sync.Once
}

// NewServiceContext 建全部依赖。任何一项建不出来都返回错误(启动致命);已建的资源在返回前关闭。
// 没有 etcd 的 mail 注册不了;没有 mmorpg_mail 或号段的 mail 每个请求都只能回 kServiceUnavailable ——
// "进程活着但全量失败"比起不来更难查,所以在启动期就失败。
func NewServiceContext(c config.Config) (*ServiceContext, error) {
	sc := &ServiceContext{Config: c}
	ok := false
	defer func() {
		if !ok {
			sc.Stop()
		}
	}()

	var err error
	sc.Etcd, err = clientv3.New(clientv3.Config{Endpoints: c.Etcd.Hosts, DialTimeout: etcdDialTimeout})
	if err != nil {
		return nil, fmt.Errorf("mail: create etcd client: %w", err)
	}

	sc.DB, err = OpenMySQL(c.MySQL)
	if err != nil {
		return nil, fmt.Errorf("mail: %w", err)
	}

	sc.SharedRedis, err = redis.NewRedis(c.Redis.RedisConf)
	if err != nil {
		return nil, fmt.Errorf("mail: shared Redis %s: %w", c.Redis.Host, err)
	}
	sc.Sessions = data.NewSessionReader(sc.SharedRedis)

	conn, err := zrpc.NewClient(c.DataServiceRpc)
	if err != nil {
		return nil, fmt.Errorf("mail: data_service client (biz_tag=%s needs AllocateIdSegment): %w", MailIDBizTag, err)
	}
	sc.dataServiceConn = conn.Conn()
	sc.DataService = dspb.NewDataServiceClient(conn.Conn())

	sc.MailIDs, err = NewMailIDSegment(c.IdSegment, sc.DataService)
	if err != nil {
		return nil, fmt.Errorf("mail: %w", err)
	}
	sc.KafkaWriter = newKafkaWriter(c)
	sc.GateCommandBuilder = mailkafka.NewGateCommandBuilder()

	ok = true
	return sc, nil
}

// NewMailIDSegment 按 IdSegment 配置建 mail_id 号段客户端(照 go/trade 的 NewListingIDSegment)。
// Enabled=false 或客户端为 nil 直接报错:mail 只由号段发号,没有 snowflake 回退。
func NewMailIDSegment(cfg idsegment.Conf, client dspb.DataServiceClient) (*idsegment.Client, error) {
	if !cfg.Enabled {
		return nil, errors.New("IdSegment.Enabled=false: mail mints mail_id / sys_mail_id only from id_segment (biz_tag=" + MailIDBizTag + ")")
	}
	if client == nil {
		return nil, errors.New("data_service client is nil: mail_id minting needs data_service.AllocateIdSegment")
	}
	return idsegment.New(
		idsegment.Adapt(client.AllocateIdSegment,
			func(bizTag string, step uint32) *dspb.AllocateIdSegmentRequest {
				return &dspb.AllocateIdSegmentRequest{BizTag: bizTag, Step: step}
			}),
		idsegment.Options{
			BizTag:  MailIDBizTag,
			Step:    cfg.StepOrDefault(),
			MinStep: cfg.MinStepOrDefault(),
			MaxStep: cfg.MaxStepOrDefault(),
		},
	)
}

// WarmMailIDSegment 启动时同步领第一段;失败只告警(之后每次 Next 会自己再试)。
func (sc *ServiceContext) WarmMailIDSegment() {
	if sc.MailIDs == nil {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), mailIDWarmTimeout)
	defer cancel()
	if err := sc.MailIDs.Warm(ctx); err != nil {
		logx.Errorf("[mail] mail_id first segment not available yet (data_service.AllocateIdSegment biz_tag=%s): %v "+
			"— sending mail fails closed with kServiceUnavailable until data_service answers; "+
			"in production also check that %s is in data_service BootstrapTags", MailIDBizTag, err, MailIDBizTag)
	}
}

// newKafkaWriter 建 S2C 推送用的 writer;没配 broker 返回 nil(见 ServiceContext.KafkaWriter)。写法与 friend 逐项相同,理由同。
func newKafkaWriter(c config.Config) *kafkago.Writer {
	if len(c.Kafka.Brokers) == 0 {
		logx.Info("[mail] Kafka.Brokers empty: S2C push disabled (new personal mail is only persisted; online recipient is not notified)")
		return nil
	}
	return &kafkago.Writer{
		Addr: kafkago.TCP(c.Kafka.Brokers...),
		// gate-cmd_gN 必须按 node_id % P 落到**指定分区**:消费端 assign 的就是那一个分区,落错分区 = 目标 gate 永远收不到,
		// 而 Kafka 一个错都不报(docs/design/control-plane-topic-partitioning-20260908.md)。Writer 只问 Balancer。
		Balancer: &kafkacmd.CommandPartitionBalancer{Fallback: &kafkago.Hash{}},
		// RequiredAcks 零值是 RequireNone(fire-and-forget):broker 端失败全部不可见。显式 RequireOne。
		RequiredAcks: kafkago.RequireOne,
		// Async=false 是 RequireOne 能生效的前提(Async 下 WriteMessages 塞进队列就返回 nil)。
		Async:        false,
		WriteTimeout: kafkaWriteTimeout,
		// BatchTimeout 零值是 1s:同步写单条消息要等满它才成批发出 —— 推送发生在回包之前,每封在线收件人的信都会
		// 白等一秒,还会挤占 pushTimeout(1.5s)让推送稳定超时。与 guild / match / player_inbox 同取 10ms。
		BatchTimeout: 10 * time.Millisecond,
		// 推送是 at-most-once 的旁路:不重试,重试只会吃掉请求预算(红点真相是 ListMails 的 unread_count)。
		MaxAttempts: 1,
	}
}

// Stop 依次关号段、Kafka writer(flush 在途批次)、data_service 连接、DB、etcd。幂等。
// 调用方须保证 noderegistry.Close() 与 killswitch 的 ctx 取消都**先于**它(二者共用 etcd 连接)。
// 不关 SharedRedis:go-zero 的 redis.Redis 没有 Close,连接池由框架持有。
func (sc *ServiceContext) Stop() {
	sc.stopOnce.Do(func() {
		if sc.MailIDs != nil {
			sc.MailIDs.Close()
		}
		if sc.KafkaWriter != nil {
			if err := sc.KafkaWriter.Close(); err != nil {
				logx.Errorf("[mail] Kafka writer close/flush failed: %v", err)
			}
		}
		if sc.dataServiceConn != nil {
			if err := sc.dataServiceConn.Close(); err != nil {
				logx.Errorf("[mail] data_service connection close failed: %v", err)
			}
		}
		if sc.DB != nil {
			if err := sc.DB.Close(); err != nil {
				logx.Errorf("[mail] MySQL close failed: %v", err)
			}
		}
		if sc.Etcd != nil {
			if err := sc.Etcd.Close(); err != nil {
				logx.Errorf("[mail] etcd client close failed: %v", err)
			}
		}
	})
}

// BuildDSN 拼 mmorpg_mail 的 DSN(与 go/friend、go/trade、go/player_inbox 同口径):
//   - sql_mode=STRICT_TRANS_TABLES:超长文本 / 越界整数报错而不是静默截断(发件校验之外的第二道防线);
//   - transaction_isolation=READ-COMMITTED:事务之外的自动提交语句(ReadMail / DeleteMails 的 UPDATE、state 行 upsert、
//     sweep 的 DELETE)也走 RC,不在 RR 下拿间隙锁;发件事务本来就固定 RC(设计 §4.1);
//   - innodb_lock_wait_timeout=2:锁等待上限 2 秒,小于整请求预算(3.5s)。发件事务要等同一收件人守卫行上的并发发件;
//     mail 对 MySQL 不做自动重试(设计 §2.5),等不到就让服务器自己放弃(1205),handler 在预算内 in-band 回
//     kServiceUnavailable —— 而不是靠 context 取消关连接,那样服务器端的锁等待会在没人等它的情况下继续占着队列。
//
// 返回值**含密码**,只能交给 sql.Open,绝不打日志、绝不进错误信息(横幅用 MySQLTarget)。
func BuildDSN(c config.MySQLConf) string {
	return fmt.Sprintf("%s:%s@tcp(%s)/%s?parseTime=true&charset=utf8mb4&sql_mode=%%27STRICT_TRANS_TABLES%%27"+
		"&transaction_isolation=%%27READ-COMMITTED%%27&innodb_lock_wait_timeout=2",
		c.User, c.Password, c.Host, c.DBName)
}

// MySQLTarget 是连接目标的可读描述(**不含密码**),供横幅与错误信息使用。
func MySQLTarget(c config.MySQLConf) string {
	return fmt.Sprintf("%s/%s (user=%s)", c.Host, c.DBName, c.User)
}

// OpenMySQL 建池、设上限、ping。返回错误时池已关闭;错误信息点名库名(D-14 第 5 条:-migrate 以 1 退出时
// 运维要能直接看出是"库不存在"而不是"密码错")。
func OpenMySQL(c config.MySQLConf) (*sql.DB, error) {
	db, err := sql.Open("mysql", BuildDSN(c))
	if err != nil {
		return nil, fmt.Errorf("open MySQL %s: %w", MySQLTarget(c), err)
	}
	// 上限必须显式设:database/sql 的 MaxOpenConns 零值是"不限",一次慢查询风暴就能把 max_connections 吃光。
	db.SetMaxOpenConns(c.MaxOpenConn)
	db.SetMaxIdleConns(c.MaxIdleConn)

	ctx, cancel := context.WithTimeout(context.Background(), mysqlPingTimeout)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		_ = db.Close()
		return nil, fmt.Errorf("ping MySQL %s failed (create database %s and grant access first, see deploy/mysql-init/00_init_zone_dbs.sql): %w",
			MySQLTarget(c), c.DBName, err)
	}
	return db, nil
}
