package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	base "proto/common/base"
	mailpb "proto/mail"
	rankpb "proto/rank"
	"rank/internal/data"
	"rank/internal/metrics"
	"rank/internal/reward"
	"rank/internal/server"
	"schemamigrate"
	"shared/authoritysql"
	"shared/buildinfo"
	"shared/domainhost"
	tables "shared/generated/table"
	"shared/serviceauth"
)

type config struct {
	domainhost.Config
	MailRpc       zrpc.RpcClientConf `json:"MailRpc"`
	MailSecretEnv string             `json:"MailSecretEnv"`
}

var errManualSchema = errors.New("领域结构需人工处理")

func main() {
	file := flag.String("f", "etc/rank.yaml", "配置文件")
	migrate := flag.Bool("migrate", false, "仅按proto迁移领域库")
	version := flag.Bool("version", false, "打印版本")
	flag.Parse()
	fmt.Println(buildinfo.StartupLine("rank"))
	if *version {
		return
	}
	var c config
	conf.MustLoad(*file, &c)
	if err := run(c, *migrate); err != nil {
		fmt.Fprintln(os.Stderr, "[rank]", err)
		code := schemamigrate.ExitCode(schemamigrate.Report{}, err)
		if errors.Is(err, errManualSchema) {
			code = schemamigrate.ExitManual
		}
		os.Exit(code)
	}
}
func run(c config, migrate bool) error {
	if err := c.Validate(); err != nil {
		return err
	}
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()
	defer logx.Close()
	db, err := authoritysql.Open(ctx, os.Getenv(c.DSNEnv), data.DatabaseName, c.RequireTiDB, c.MaxOpenConns)
	if err != nil {
		return err
	}
	defer db.Close()
	options := schemamigrate.Options{Database: data.DatabaseName, Tables: data.Tables(), Logf: logx.Infof}
	var report schemamigrate.Report
	if migrate || c.AutoMigrate {
		report, err = schemamigrate.Up(ctx, db, options)
	} else {
		report, err = schemamigrate.Plan(ctx, db, options)
	}
	if err != nil {
		return err
	}
	if len(report.Manual) > 0 {
		return fmt.Errorf("%w: %s", errManualSchema, strings.Join(report.Manual, ";"))
	}
	if !migrate && !c.AutoMigrate && !report.Clean() {
		return errors.New("排行榜结构未部署，请先运行rank -migrate")
	}
	for _, warning := range report.Warnings {
		if strings.Contains(warning, "索引") {
			return errors.New("排行榜必要索引缺失")
		}
		logx.Info(warning)
	}
	if migrate {
		return nil
	}
	metrics.Register()
	secrets, err := c.Secrets()
	if err != nil {
		return err
	}
	auth, err := serviceauth.New(secrets)
	if err != nil {
		return err
	}
	mailSecret := os.Getenv(c.MailSecretEnv)
	if len(strings.TrimSpace(mailSecret)) < 32 {
		return errors.New("排行榜邮件密钥未注入")
	}
	if err = tables.ItemTableManagerInstance.Load(c.TableDir, false); err != nil {
		return err
	}
	ids, err := domainhost.OpenIDs(ctx, c.DataServiceRpc, "rank_board", "rank_settlement", "rank_reward")
	if err != nil {
		return err
	}
	defer ids.Close()
	repo := data.New(db, ids.IDs["rank_board"].Next, ids.IDs["rank_settlement"].Next, ids.IDs["rank_reward"].Next, func(id uint32) bool { _, ok := tables.ItemTableManagerInstance.FindById(id); return ok })
	handler := server.New(repo, auth, ids.Data)
	mailRPC, err := zrpc.NewClient(c.MailRpc)
	if err != nil {
		return err
	}
	defer mailRPC.Conn().Close()
	worker := &reward.Worker{Repo: repo, Mail: mailpb.NewMailAdminClient(mailRPC.Conn()), Secret: mailSecret}
	workerCtx, cancelWorker := context.WithCancel(ctx)
	finished := make(chan struct{})
	go func() { defer close(finished); worker.Run(workerCtx) }()
	defer func() {
		cancelWorker()
		budget := time.NewTimer(4 * time.Second)
		defer budget.Stop()
		select {
		case <-finished:
		case <-budget.C:
			logx.Error("[rank] 奖励工作器关闭超过预算，未声明排空")
		}
	}()
	return domainhost.Serve(ctx, c.Config, base.ENodeType_RankNodeService, server.ClientMethods(), func(s *grpc.Server) {
		rankpb.RegisterClientPlayerRankServer(s, handler)
		rankpb.RegisterRankAdminServer(s, handler)
	})
}
