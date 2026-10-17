package main

import (
	"context"
	"crypto/rand"
	"database/sql"
	"encoding/base64"
	"encoding/hex"
	"net"
	"os"
	"sync/atomic"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"mail/internal/data"
	"mail/internal/server"
	base "proto/common/base"
	mailpb "proto/mail"
	"schemamigrate"
	"shared/authoritysql"
	"shared/domainhost"
	"shared/serviceauth"
)

type committedReminder struct {
	db    *sql.DB
	calls atomic.Int64
	t     *testing.T
}

func (n *committedReminder) Personal(ctx context.Context, player, mail uint64) {
	var count uint64
	if err := n.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM mail_message WHERE mail_id=? AND owner_player_id=?", mail, player).Scan(&count); err != nil || count != 1 {
		n.t.Errorf("提醒早于SQL提交: count=%d err=%v", count, err)
	}
	n.calls.Add(1)
}

func TestTiDBRealRPCAuthSendOfflineReadClaimAndDeleteProtection(t *testing.T) {
	if os.Getenv("MAIL_RANK_TEST_TIDB") != "127.0.0.1:4000" {
		t.Skip("仅本机4000随机库真实SQL与loopback gRPC")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	var random [8]byte
	if _, err := rand.Read(random[:]); err != nil {
		t.Fatal(err)
	}
	database := "mail_rpc_it_" + hex.EncodeToString(random[:])
	adminDB, err := sql.Open("mysql", "root@tcp(127.0.0.1:4000)/?timeout=1s&readTimeout=5s&writeTimeout=5s")
	if err != nil {
		t.Fatal(err)
	}
	defer adminDB.Close()
	if _, err = adminDB.ExecContext(ctx, "CREATE DATABASE "+database); err != nil {
		t.Fatal(err)
	}
	defer func() {
		clean, stop := context.WithTimeout(context.Background(), 5*time.Second)
		defer stop()
		if _, e := adminDB.ExecContext(clean, "DROP DATABASE "+database); e != nil {
			t.Errorf("清RPC随机库: %v", e)
		}
	}()
	db, err := authoritysql.Open(ctx, "root@tcp(127.0.0.1:4000)/"+database, database, true, 8)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	if report, e := schemamigrate.Up(ctx, db, schemamigrate.Options{Database: database, Tables: data.Tables()}); e != nil || len(report.Manual) > 0 {
		t.Fatalf("schema: %v %+v", e, report)
	}
	if _, err = db.ExecContext(ctx, "CREATE TABLE guild_member (guild_id BIGINT UNSIGNED NOT NULL,player_id BIGINT UNSIGNED NOT NULL,PRIMARY KEY(guild_id,player_id))"); err != nil {
		t.Fatal(err)
	}
	var ids atomic.Uint64
	next := func(context.Context) (uint64, error) { return ids.Add(1), nil }
	repo := data.New(db, next, next, func(uint32) bool { return true }, database)
	const secret = "mail-rpc-integration-only-never-a-real-secret"
	verifier, err := serviceauth.New(map[string]string{"rank": secret})
	if err != nil {
		t.Fatal(err)
	}
	reminder := &committedReminder{db: db, t: t}
	handler := server.New(repo, verifier, reminder)
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	rpc := grpc.NewServer(grpc.UnaryInterceptor(domainhost.Guard(server.ClientMethods(), 3*time.Second)))
	mailpb.RegisterMailAdminServer(rpc, handler)
	mailpb.RegisterClientPlayerMailServer(rpc, handler)
	go func() { _ = rpc.Serve(listener) }()
	defer rpc.Stop()
	connection, err := grpc.NewClient(listener.Addr().String(), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatal(err)
	}
	defer connection.Close()
	sender := mailpb.NewMailAdminClient(connection)
	client := mailpb.NewClientPlayerMailClient(connection)
	request := &mailpb.SendMailRequest{SenderKind: 3, RequestId: 1, RecipientPlayerId: 7, Title: "真实RPC奖励", Body: "离线投递到SQL", Attachments: &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{Amount: 10}}}}
	if _, err = sender.SendPersonalMail(ctx, request); status.Code(err) != codes.Unauthenticated {
		t.Fatalf("无凭据不得发信: %v", err)
	}
	if err = serviceauth.Sign(mailpb.MailAdmin_SendPersonalMail_FullMethodName, request, "rank", secret, uint64(time.Now().UnixMilli())); err != nil {
		t.Fatal(err)
	}
	sent, err := sender.SendPersonalMail(ctx, request)
	if err != nil || sent.GetErrorMessage().GetId() != 0 || sent.GetMailId() == 0 {
		t.Fatalf("内部真实发件: %v %+v", err, sent)
	}
	replayed, e := sender.SendPersonalMail(ctx, request)
	if e != nil || !replayed.Duplicated || reminder.calls.Load() != 1 {
		t.Fatalf("提交后只提醒一次，重放不得新推: %+v %v count=%d", replayed, e, reminder.calls.Load())
	}
	tampered := proto.Clone(request).(*mailpb.SendMailRequest)
	tampered.RecipientPlayerId = 8
	if _, err = sender.SendPersonalMail(ctx, tampered); status.Code(err) != codes.Unauthenticated {
		t.Fatalf("签名不能换收件人: %v", err)
	}
	session, _ := proto.Marshal(&base.SessionDetails{PlayerId: 7})
	playerCtx := metadata.NewOutgoingContext(ctx, metadata.Pairs(domainhost.SessionMetadata, base64.StdEncoding.EncodeToString(session)))
	if _, err = sender.SendPersonalMail(playerCtx, request); status.Code(err) != codes.PermissionDenied {
		t.Fatalf("客户端metadata不能进入Admin: %v", err)
	}
	if _, err = client.NotifyMailChanged(playerCtx, &mailpb.MailChangedEvent{MailId: sent.MailId}); status.Code(err) != codes.PermissionDenied {
		t.Fatalf("S2C入口不可被客户端伪造: %v", err)
	}
	listed, err := client.ListMails(playerCtx, &mailpb.ListMailsRequest{})
	if err != nil || listed.GetErrorMessage().GetId() != 0 || len(listed.Mails) != 1 {
		t.Fatalf("真实会话读取: %v %+v", err, listed)
	}
	claimed, err := client.ClaimMailAttachments(playerCtx, &mailpb.ClaimMailAttachmentsRequest{MailIds: []uint64{sent.MailId}})
	if err != nil || len(claimed.Results) != 1 || claimed.Results[0].ClaimState != mailpb.MailClaimState_MAIL_CLAIM_PENDING {
		t.Fatalf("实际领取入口: %v %+v", err, claimed)
	}
	again, err := client.ClaimMailAttachments(playerCtx, &mailpb.ClaimMailAttachmentsRequest{MailIds: []uint64{sent.MailId}})
	if err != nil || again.Results[0].AssetOpId != claimed.Results[0].AssetOpId {
		t.Fatalf("RPC重放应同op: %v %+v", err, again)
	}
	deleted, err := client.DeleteMails(playerCtx, &mailpb.DeleteMailsRequest{MailIds: []uint64{sent.MailId}})
	if err != nil || len(deleted.ProtectedMailIds) != 1 || len(deleted.DeletedMailIds) != 0 {
		t.Fatalf("RPC不能删除待到账邮件: %v %+v", err, deleted)
	}
}
