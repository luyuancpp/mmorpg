// Command mailadmin直连现役M2 MailAdmin；只从环境变量读取admin角色密钥，不接受旧共享token。
// 超时后必须重放相同request-id、expire-ms及正文；系统附件先用原冻结名单执行import/seal。
package main

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
	"io"
	"os"
	base "proto/common/base"
	mailpb "proto/mail"
	tip "shared/generated/tip"
	"shared/serviceauth"
	"strconv"
	"strings"
	"time"
)

const (
	exitOK       = 0
	exitError    = 1
	exitUsage    = 2
	exitRejected = 3
)

type multiFlag []string

func (f *multiFlag) String() string         { return strings.Join(*f, ",") }
func (f *multiFlag) Set(value string) error { *f = append(*f, value); return nil }
func main()                                 { os.Exit(run(os.Args[1:], os.Getenv, os.Stdout, os.Stderr)) }
func run(args []string, getenv func(string) string, out, errOut io.Writer) int {
	if len(args) == 0 {
		return exitUsage
	}
	kind := args[0]
	methods := map[string]string{"personal": mailpb.MailAdmin_SendPersonalMail_FullMethodName, "system": mailpb.MailAdmin_SendSystemMail_FullMethodName, "import": mailpb.MailAdmin_ImportMailRecipients_FullMethodName, "seal": mailpb.MailAdmin_SealMailCampaign_FullMethodName}
	method, ok := methods[kind]
	if !ok {
		fmt.Fprintln(errOut, "用法: mailadmin personal|system|import|seal [参数]")
		return exitUsage
	}
	fs := flag.NewFlagSet("mailadmin "+kind, flag.ContinueOnError)
	fs.SetOutput(errOut)
	target := fs.String("target", "127.0.0.1:50900", "直连MailAdmin地址")
	secretEnv := fs.String("secret-env", "MAIL_PRODUCER_SECRET_ADMIN", "admin签名密钥的环境变量名，禁止传密钥值")
	file := fs.String("file", "", "现役请求proto JSON文件；冻结名单操作必须提供")
	to := fs.Uint64("to", 0, "个人收件人ID")
	title := fs.String("title", "", "标题")
	body := fs.String("body", "", "正文")
	requestID := fs.Uint64("request-id", 0, "重试必须使用首次请求号")
	expire := fs.Uint64("expire-ms", 0, "重试必须使用首次过期时刻")
	days := fs.Int("expire-days", 7, "未填expire-ms时的有效天数")
	timeout := fs.Duration("timeout", 10*time.Second, "本次调用预算")
	var currency, item multiFlag
	fs.Var(&currency, "currency", "type:amount，可重复")
	fs.Var(&item, "item", "config_id:count，可重复")
	if err := fs.Parse(args[1:]); err != nil {
		return exitUsage
	}
	if fs.NArg() != 0 || *timeout <= 0 || *timeout > time.Minute || *days < 1 || *days > 365 {
		return exitUsage
	}
	secret := strings.TrimSpace(getenv(*secretEnv))
	if len([]byte(secret)) < 32 {
		fmt.Fprintln(errOut, "admin签名密钥未注入或不足32字节")
		return exitUsage
	}
	var request proto.Message
	switch kind {
	case "import":
		request = &mailpb.ImportMailRecipientsRequest{}
	case "seal":
		request = &mailpb.SealMailCampaignRequest{}
	default:
		request = &mailpb.SendMailRequest{SenderKind: 1, RequestId: *requestID, RecipientPlayerId: *to, Title: *title, Body: *body, ExpireMs: *expire}
	}
	if *file != "" {
		source, err := os.Open(*file)
		if err != nil {
			fmt.Fprintln(errOut, "请求文件不可读")
			return exitUsage
		}
		bytes, err := io.ReadAll(io.LimitReader(source, (4<<20)+1))
		_ = source.Close()
		if err != nil {
			fmt.Fprintln(errOut, "请求文件不可读")
			return exitUsage
		}
		if len(bytes) > 4<<20 {
			fmt.Fprintln(errOut, "请求文件超限")
			return exitUsage
		}
		if err := protojson.Unmarshal(bytes, request); err != nil {
			fmt.Fprintln(errOut, "请求JSON不符合现役协议")
			return exitUsage
		}
	} else if kind == "import" || kind == "seal" {
		fmt.Fprintln(errOut, "名单导入/封存必须提供-file")
		return exitUsage
	}
	switch in := request.(type) {
	case *mailpb.SendMailRequest:
		if in.SenderKind != 0 && in.SenderKind != 1 {
			return exitUsage
		}
		in.SenderKind = 1
		if in.RequestId == 0 {
			var bytes [8]byte
			if _, err := rand.Read(bytes[:]); err != nil {
				return exitError
			}
			in.RequestId = binary.BigEndian.Uint64(bytes[:])
			if in.RequestId == 0 {
				in.RequestId = 1
			}
		}
		if in.ExpireMs == 0 {
			in.ExpireMs = uint64(time.Now().Add(time.Duration(*days) * 24 * time.Hour).UnixMilli())
		}
		if len(currency) > 0 || len(item) > 0 {
			if in.Attachments == nil {
				in.Attachments = &mailpb.MailAttachments{}
			}
			for _, value := range currency {
				a, b, err := pair(value, 64)
				if err != nil {
					return exitUsage
				}
				in.Attachments.Currencies = append(in.Attachments.Currencies, &mailpb.MailCurrency{CurrencyType: uint32(a), Amount: b})
			}
			for _, value := range item {
				a, b, err := pair(value, 32)
				if err != nil {
					return exitUsage
				}
				in.Attachments.Items = append(in.Attachments.Items, &mailpb.MailItem{ConfigId: uint32(a), Count: uint32(b)})
			}
		}
		fmt.Fprintf(out, "request_id=%d expire_ms=%d（未知结果按相同请求原样重试）\n", in.RequestId, in.ExpireMs)
	case *mailpb.ImportMailRecipientsRequest:
		if in.SenderKind != 0 && in.SenderKind != 1 {
			return exitUsage
		}
		in.SenderKind = 1
	case *mailpb.SealMailCampaignRequest:
		if in.SenderKind != 0 && in.SenderKind != 1 {
			return exitUsage
		}
		in.SenderKind = 1
	}
	if err := serviceauth.Sign(method, request, "admin", secret, uint64(time.Now().UnixMilli())); err != nil {
		return exitUsage
	}
	conn, err := grpc.NewClient(*target, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return exitError
	}
	defer conn.Close()
	client := mailpb.NewMailAdminClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), *timeout)
	defer cancel()
	var response proto.Message
	switch kind {
	case "personal":
		response, err = client.SendPersonalMail(ctx, request.(*mailpb.SendMailRequest))
	case "system":
		response, err = client.SendSystemMail(ctx, request.(*mailpb.SendMailRequest))
	case "import":
		response, err = client.ImportMailRecipients(ctx, request.(*mailpb.ImportMailRecipientsRequest))
	case "seal":
		response, err = client.SealMailCampaign(ctx, request.(*mailpb.SealMailCampaignRequest))
	}
	if err != nil {
		fmt.Fprintf(errOut, "调用失败，结果未知 code=%s；保留原请求重试\n", status.Code(err))
		return exitError
	}
	reply, ok := response.(interface{ GetErrorMessage() *base.TipInfoMessage })
	if !ok {
		return exitError
	}
	if code := reply.GetErrorMessage().GetId(); code != 0 {
		fmt.Fprintf(errOut, "业务码=%d\n", code)
		if tip.IsFault(code) {
			return exitError
		}
		return exitRejected
	}
	encoded, err := protojson.Marshal(response)
	if err != nil {
		return exitError
	}
	fmt.Fprintln(out, string(encoded))
	return exitOK
}
func pair(value string, bits int) (uint64, uint64, error) {
	parts := strings.Split(value, ":")
	if len(parts) != 2 {
		return 0, 0, errors.New("需要type:amount")
	}
	a, err := strconv.ParseUint(parts[0], 10, 32)
	if err != nil {
		return 0, 0, err
	}
	b, err := strconv.ParseUint(parts[1], 10, bits)
	if err != nil || b == 0 {
		return 0, 0, errors.New("数量必须为正")
	}
	return a, b, nil
}
