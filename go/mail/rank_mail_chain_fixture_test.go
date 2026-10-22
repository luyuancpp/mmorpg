package main

import (
	"bufio"
	"context"
	"errors"
	"net"
	"os"
	"regexp"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"mail/internal/data"
	"mail/internal/server"
	guildpb "proto/guild"
	mailpb "proto/mail"
	"schemamigrate"
	"shared/authoritysql"
	"shared/domainhost"
	"shared/loginsession"
	"shared/serviceauth"
)

// TestRankMailChainFixture 仅供跨模块整链测试的独立子进程装配真实邮件服务。
// 随机库的创建和删除归父进程；本进程既不伪造邮件来源，也不写资产终态。
func TestRankMailChainFixture(t *testing.T) {
	if os.Getenv("RANK_MAIL_CHAIN_FIXTURE") != "mail" {
		t.Skip("仅供本机 TiDB 排行到邮件整链子进程")
	}
	address := os.Getenv("RANK_MAIL_CHAIN_IT_TIDB")
	if err := mailMainFixtureEndpoint(address); err != nil {
		t.Fatal(err)
	}
	database := os.Getenv("RANK_MAIL_CHAIN_DATABASE")
	if !regexp.MustCompile(`^rank_mail_chain_[a-f0-9]{16}_mail$`).MatchString(database) {
		t.Fatal("整链随机库名无效")
	}
	secret := strings.TrimSpace(os.Getenv("RANK_MAIL_CHAIN_SECRET"))
	if len([]byte(secret)) < 32 {
		t.Fatal("整链内部调用密钥未配置或不足 32 字节")
	}
	readyFile := os.Getenv("RANK_MAIL_CHAIN_READY_FILE")
	if readyFile == "" {
		t.Fatal("整链就绪文件未配置")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	db, err := authoritysql.Open(ctx, "root@tcp("+address+")/"+database, database, true, 8)
	if err != nil {
		t.Fatalf("打开邮件强读连接: %v", err)
	}
	t.Cleanup(func() {
		if err := db.Close(); err != nil {
			t.Errorf("关闭邮件连接: %v", err)
		}
	})
	tables := append(data.Tables(), &guildpb.GuildMemberRecord{})
	if report, err := schemamigrate.Up(ctx, db, schemamigrate.Options{Database: database, Tables: tables}); err != nil || len(report.Manual) > 0 {
		t.Fatalf("邮件整链 schema: %v %+v", err, report)
	}
	var ids atomic.Uint64
	next := func(context.Context) (uint64, error) { return ids.Add(1), nil }
	repo := data.New(db, next, next, func(uint32) bool { return true }, database)
	// 全局角色目录与原登录态是客户端列表的真实依赖；只在此随机夹具库种原件。
	if _, err := db.ExecContext(ctx, "CREATE TABLE account_role (player_id BIGINT UNSIGNED PRIMARY KEY,account VARBINARY(191) NOT NULL,owner_revision BIGINT UNSIGNED NOT NULL,trade_listing_id BIGINT UNSIGNED NOT NULL)"); err != nil {
		t.Fatal(err)
	}
	for _, player := range []uint64{7, 8} {
		if _, err := db.ExecContext(ctx, "INSERT INTO account_role VALUES (?, ?,3,0)", player, []byte("整链原账号")); err != nil {
			t.Fatal(err)
		}
	}
	if err := repo.ConfigureRoleAuthority(ctx, database); err != nil {
		t.Fatal(err)
	}
	loginCache := miniredis.RunT(t)
	loginCache.Set(loginsession.KeyFromMember("mail-rank-chain-original:19"), "整链原账号")
	loginReader := redis.NewClient(&redis.Options{Addr: loginCache.Addr()})
	t.Cleanup(func() { _ = loginReader.Close() })
	auth, err := serviceauth.New(map[string]string{"rank": secret})
	if err != nil {
		t.Fatal("整链邮件调用方配置无效")
	}
	handler := server.New(repo, auth).WithLoginSessions(loginReader)
	var responseLost atomic.Bool
	loseFirstCommittedReply := func(ctx context.Context, request any, info *grpc.UnaryServerInfo, next grpc.UnaryHandler) (any, error) {
		response, err := next(ctx, request)
		if err != nil || info.FullMethod != mailpb.MailAdmin_SendPersonalMail_FullMethodName {
			return response, err
		}
		sent, ok := response.(*mailpb.SendMailResponse)
		if ok && sent.GetErrorMessage().GetId() == 0 && sent.GetMailId() != 0 && responseLost.CompareAndSwap(false, true) {
			// 生产入口返回成功即已提交 SQL；只丢掉回包，重放仍走原生产入口。
			return nil, status.Error(codes.Unavailable, "整链故障注入：邮件已提交但回包丢失")
		}
		return response, nil
	}
	rpc := grpc.NewServer(grpc.ChainUnaryInterceptor(domainhost.Guard(server.ClientMethods(), 3*time.Second), loseFirstCommittedReply))
	mailpb.RegisterMailAdminServer(rpc, handler)
	mailpb.RegisterClientPlayerMailServer(rpc, handler)
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("监听邮件整链入口: %v", err)
	}
	serveDone := make(chan struct{})
	var serveErr error
	go func() {
		defer close(serveDone)
		serveErr = rpc.Serve(listener)
	}()
	t.Cleanup(func() {
		rpc.Stop()
		_ = listener.Close()
		select {
		case <-serveDone:
			if serveErr != nil && !errors.Is(serveErr, grpc.ErrServerStopped) && !errors.Is(serveErr, net.ErrClosed) {
				t.Errorf("邮件整链服务退出: %v", serveErr)
			}
		case <-time.After(3 * time.Second):
			t.Error("邮件整链服务未在预算内退出")
		}
	})
	// 完成一次真实 HTTP/2 建连后才发布 endpoint，父进程无需猜测启动等待。
	readyCtx, stopReady := context.WithTimeout(ctx, 3*time.Second)
	connection, err := grpc.DialContext(readyCtx, listener.Addr().String(), grpc.WithTransportCredentials(insecure.NewCredentials()), grpc.WithBlock())
	stopReady()
	if err != nil {
		t.Fatalf("邮件整链入口未就绪: %v", err)
	}
	if err := connection.Close(); err != nil {
		t.Fatalf("关闭邮件就绪探针: %v", err)
	}
	file, err := os.OpenFile(readyFile, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		t.Fatalf("创建邮件就绪文件: %v", err)
	}
	_, writeErr := file.WriteString(listener.Addr().String())
	closeErr := file.Close()
	if err := errors.Join(writeErr, closeErr); err != nil {
		t.Fatalf("发布邮件就绪入口: %v", err)
	}

	inputDone := make(chan struct{})
	var inputErr error
	go func() {
		defer close(inputDone)
		scanner := bufio.NewScanner(os.Stdin)
		for scanner.Scan() {
			if strings.TrimSpace(scanner.Text()) == "stop" {
				return
			}
		}
		inputErr = scanner.Err()
	}()
	t.Cleanup(func() {
		// 本测试只在独立子进程运行；关闭自己的输入管道以解除超时后的 Scan。
		_ = os.Stdin.Close()
		select {
		case <-inputDone:
		case <-time.After(3 * time.Second):
			t.Error("邮件整链输入监听未在预算内退出")
		}
	})
	select {
	case <-inputDone:
		if inputErr != nil {
			t.Fatalf("读取整链停止指令: %v", inputErr)
		}
	case <-serveDone:
		t.Fatalf("邮件整链服务提前退出: %v", serveErr)
	case <-ctx.Done():
		t.Fatal("邮件整链子进程超过 90 秒预算")
	}
}
