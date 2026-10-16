// Command mailadmin 是 mail 的运维发件工具(设计 docs/design/mail-system.md §2.2):直连 mail 的 gRPC,调 MailAdmin 的
// SendPersonalMail / SendSystemMail。与 mail 服务同 module 的第二个 main,不另起 go.mod。
//
// 用法:
//
//	mailadmin personal -to <player_id> -title <标题> -body <正文> [-sender-name 运营] [-expire-days 7]
//	mailadmin system   -title <标题> -body <正文> [-sender-name 运营] [-expire-days 7]
//
// 公共参数:-target host:port(默认 127.0.0.1:50900)、-request-id、-expire-ms、-currency type:amount(可重复)、
// -item config_id:count(可重复)、-operator、-timeout。
//
// 鉴权:令牌只从环境变量 MMORPG_MAIL_ADMIN_TOKEN 读(与 mail 进程同一个变量,去首尾空白后 ≥ 32 字节),
// 放进 gRPC metadata x-mail-admin-token。没有命令行参数可以传令牌:命令行会进 shell 历史与进程列表。
//
// 重试:每次发件都打印 request_id 与 expire_ms。结果未知(超时 / 断连)时,**用同一组 -request-id 与 -expire-ms 重跑**,
// 内容不变即得到首次的结果(duplicated=true),不会多发一封;换了内容会被拒(MailRequestConflict)。
// 重试窗口必须远小于 expire_ms + 服务端保留期(键在那之后会被 sweep 物理删除,§2.4)。
//
// 退出码:0 已发出或重放命中;1 调用失败或服务端故障(连不上、超时、PermissionDenied、in-band 的 kServiceUnavailable ——
// 后者里有"提交了但回包前出错"的情形,结果未知,必须用同一组 -request-id / -expire-ms 重试,不要换新 id 重发);
// 2 参数不合法或缺令牌(没有发出任何请求);3 服务端业务拒绝(满箱、冲突、附件不合法等 in-band tip 码,见输出;重发同样会被拒)。
package main

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"

	"mail/internal/config"

	base "proto/common/base"
	mailpb "proto/mail"

	"shared/generated/pb/table"
)

const (
	exitOK       = 0
	exitError    = 1
	exitUsage    = 2
	exitRejected = 3
)

const (
	defaultTarget     = "127.0.0.1:50900"
	defaultExpireDays = 7
	defaultTimeout    = 10 * time.Second
	dayMs             = int64(24 * time.Hour / time.Millisecond)
)

func main() {
	os.Exit(run(os.Args[1:], os.Getenv, os.Stdout, os.Stderr))
}

// run 是可测的主体:参数、环境变量与输出都由调用方注入。
func run(args []string, getenv func(string) string, stdout, stderr io.Writer) int {
	if len(args) == 0 || (args[0] != "personal" && args[0] != "system") {
		fmt.Fprintln(stderr, "usage: mailadmin personal|system [flags]   (run with -h after the subcommand for flags)")
		return exitUsage
	}
	kind := args[0]
	opts, err := parseFlags(kind, args[1:], stderr)
	if err != nil {
		if !errors.Is(err, flag.ErrHelp) {
			fmt.Fprintf(stderr, "mailadmin: %v\n", err)
		}
		return exitUsage
	}
	token := config.ResolveAdminToken(getenv)
	if token == "" {
		fmt.Fprintf(stderr, "mailadmin: %s is not set or shorter than %d bytes after trimming; refusing to call MailAdmin\n",
			config.AdminTokenEnv, config.MinAdminTokenBytes)
		return exitUsage
	}
	if opts.operator == "" {
		opts.operator = firstNonEmpty(getenv("USERNAME"), getenv("USER"), "unknown")
	}
	if opts.requestID == 0 {
		id, err := randomRequestID()
		if err != nil {
			fmt.Fprintf(stderr, "mailadmin: generate request_id: %v\n", err)
			return exitError
		}
		opts.requestID = id
	}
	if opts.expireMs == 0 {
		opts.expireMs = uint64(time.Now().UnixMilli() + int64(opts.expireDays)*dayMs)
	}
	// 先打印重试所需的两个值:请求发出之后才失败(超时 / 断连)时,调用方要靠它们原样重放。
	fmt.Fprintf(stdout, "request_id=%d expire_ms=%d (re-run with -request-id %d -expire-ms %d to retry safely)\n",
		opts.requestID, opts.expireMs, opts.requestID, opts.expireMs)

	conn, err := grpc.NewClient(opts.target, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		fmt.Fprintf(stderr, "mailadmin: dial %s: %v\n", opts.target, err)
		return exitError
	}
	defer func() { _ = conn.Close() }()
	client := mailpb.NewMailAdminClient(conn)

	ctx, cancel := context.WithTimeout(context.Background(), opts.timeout)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, config.AdminTokenMetadataKey, token, "x-operator", opts.operator)

	switch kind {
	case "personal":
		resp, err := client.SendPersonalMail(ctx, &mailpb.SendPersonalMailRequest{
			SenderKind:        mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL,
			RequestId:         opts.requestID,
			RecipientPlayerId: opts.to,
			SenderName:        opts.senderName,
			Title:             opts.title,
			Body:              opts.body,
			Attachments:       opts.attachments,
			ExpireMs:          opts.expireMs,
		})
		if err != nil {
			fmt.Fprintf(stderr, "mailadmin: SendPersonalMail failed (result unknown; retry with the same -request-id / -expire-ms): %v\n", err)
			return exitError
		}
		if tip := resp.GetErrorMessage(); tip.GetId() != 0 {
			return reportTip(stderr, "SendPersonalMail", tip)
		}
		fmt.Fprintf(stdout, "OK mail_id=%d duplicated=%t\n", resp.GetMailId(), resp.GetDuplicated())
	case "system":
		resp, err := client.SendSystemMail(ctx, &mailpb.SendSystemMailRequest{
			SenderKind:  mailpb.MailSenderKind_MAIL_SENDER_KIND_OPS_TOOL,
			RequestId:   opts.requestID,
			SenderName:  opts.senderName,
			Title:       opts.title,
			Body:        opts.body,
			Attachments: opts.attachments,
			ExpireMs:    opts.expireMs,
		})
		if err != nil {
			fmt.Fprintf(stderr, "mailadmin: SendSystemMail failed (result unknown; retry with the same -request-id / -expire-ms): %v\n", err)
			return exitError
		}
		if tip := resp.GetErrorMessage(); tip.GetId() != 0 {
			return reportTip(stderr, "SendSystemMail", tip)
		}
		fmt.Fprintf(stdout, "OK sys_mail_id=%d duplicated=%t\n", resp.GetSysMailId(), resp.GetDuplicated())
	}
	return exitOK
}

// options 是解析后的命令行参数。
type options struct {
	target      string
	to          uint64
	senderName  string
	title       string
	body        string
	expireDays  int
	expireMs    uint64
	requestID   uint64
	attachments *mailpb.MailAttachments
	operator    string
	timeout     time.Duration
}

// parseFlags 解析子命令参数。只做本地能判的形状检查;长度、白名单等规则以服务端为准(这里不复制第二份)。
func parseFlags(kind string, args []string, stderr io.Writer) (options, error) {
	fs := flag.NewFlagSet("mailadmin "+kind, flag.ContinueOnError)
	fs.SetOutput(stderr)
	var opts options
	var currencies, items multiFlag
	fs.StringVar(&opts.target, "target", defaultTarget, "mail gRPC address host:port (direct, not through gate)")
	if kind == "personal" {
		fs.Uint64Var(&opts.to, "to", 0, "recipient player_id (required)")
	}
	fs.StringVar(&opts.senderName, "sender-name", "运营", "display signature shown to players (not an identity)")
	fs.StringVar(&opts.title, "title", "", "mail title (required)")
	fs.StringVar(&opts.body, "body", "", "mail body")
	fs.IntVar(&opts.expireDays, "expire-days", defaultExpireDays, "days until the mail expires (ignored when -expire-ms is set)")
	fs.Uint64Var(&opts.expireMs, "expire-ms", 0, "absolute expire time in unix ms; set it together with -request-id when retrying")
	fs.Uint64Var(&opts.requestID, "request-id", 0, "idempotency key; 0 = generate a random one")
	fs.Var(&currencies, "currency", "attachment currency as type:amount (repeatable; only accepted when mail runs in dev/test)")
	fs.Var(&items, "item", "attachment item as config_id:count (repeatable; only accepted when mail runs in dev/test)")
	fs.StringVar(&opts.operator, "operator", "", "operator name recorded in mail logs (default: OS user)")
	fs.DurationVar(&opts.timeout, "timeout", defaultTimeout, "per-call deadline")
	if err := fs.Parse(args); err != nil {
		return options{}, err
	}
	if fs.NArg() > 0 {
		return options{}, fmt.Errorf("unexpected arguments: %v", fs.Args())
	}
	if kind == "personal" && opts.to == 0 {
		return options{}, errors.New("-to <player_id> is required")
	}
	if strings.TrimSpace(opts.title) == "" {
		return options{}, errors.New("-title is required")
	}
	if opts.expireMs == 0 && opts.expireDays <= 0 {
		return options{}, errors.New("-expire-days must be positive")
	}
	if opts.timeout <= 0 {
		return options{}, errors.New("-timeout must be positive")
	}
	attachments, err := parseAttachments(currencies, items)
	if err != nil {
		return options{}, err
	}
	opts.attachments = attachments
	return opts, nil
}

// parseAttachments 把 type:amount / config_id:count 解析成协议形状;没有任何条目返回 nil。
func parseAttachments(currencies, items []string) (*mailpb.MailAttachments, error) {
	if len(currencies) == 0 && len(items) == 0 {
		return nil, nil
	}
	out := &mailpb.MailAttachments{}
	for _, c := range currencies {
		typ, amount, err := parsePair(c, 32, 64)
		if err != nil {
			return nil, fmt.Errorf("-currency %q: %w", c, err)
		}
		out.Currencies = append(out.Currencies, &mailpb.MailCurrency{CurrencyType: uint32(typ), Amount: amount})
	}
	for _, it := range items {
		id, count, err := parsePair(it, 32, 32)
		if err != nil {
			return nil, fmt.Errorf("-item %q: %w", it, err)
		}
		out.Items = append(out.Items, &mailpb.MailItem{ConfigId: uint32(id), Count: uint32(count)})
	}
	return out, nil
}

// parsePair 解析 "a:b" 两个无符号整数,位宽分别为 bitsA / bitsB。
func parsePair(s string, bitsA, bitsB int) (uint64, uint64, error) {
	left, right, ok := strings.Cut(s, ":")
	if !ok {
		return 0, 0, errors.New("expected the form a:b")
	}
	a, err := strconv.ParseUint(strings.TrimSpace(left), 10, bitsA)
	if err != nil {
		return 0, 0, err
	}
	b, err := strconv.ParseUint(strings.TrimSpace(right), 10, bitsB)
	if err != nil {
		return 0, 0, err
	}
	return a, b, nil
}

// randomRequestID 用密码学随机数生成非 0 的幂等键(设计 §2.4:CLI 用随机数,机器调用方用可重算的确定值)。
// 只取 63 位:有些下游把它当有符号整数打印 / 存储,避免出现负数。
func randomRequestID() (uint64, error) {
	var b [8]byte
	for {
		if _, err := rand.Read(b[:]); err != nil {
			return 0, err
		}
		if id := binary.BigEndian.Uint64(b[:]) >> 1; id != 0 {
			return id, nil
		}
	}
}

// reportTip 打印 in-band 结果码并给出退出码。kServiceUnavailable 是 mail 唯一的故障码,其中包含
// "事务已提交、回包之前出错"的情形 —— 结果未知,按调用失败处理(退出 1、提示原样重试),
// 不能当成"被拒绝",否则运维会换一个新的 request_id 重发,多发出一封(系统邮件不可撤回)。
func reportTip(stderr io.Writer, method string, tip *base.TipInfoMessage) int {
	if tip.GetId() == uint32(table.CommonError_kServiceUnavailable) {
		fmt.Fprintf(stderr, "mailadmin: %s hit a server-side fault, result unknown (retry with the same -request-id / -expire-ms): %s\n",
			method, describeTip(tip))
		return exitError
	}
	fmt.Fprintf(stderr, "mailadmin: rejected: %s\n", describeTip(tip))
	return exitRejected
}

// describeTip 把 in-band 结果码翻成可读串:已知的 MailError / CommonError 打出枚举名。
func describeTip(tip *base.TipInfoMessage) string {
	id := int32(tip.GetId())
	name := table.MailError_name[id]
	if name == "" {
		name = table.CommonError_name[id]
	}
	if name == "" {
		name = "unknown"
	}
	return fmt.Sprintf("tip=%d (%s) %s", tip.GetId(), name, strings.Join(tip.GetParameters(), " "))
}

func firstNonEmpty(values ...string) string {
	for _, v := range values {
		if v != "" {
			return v
		}
	}
	return ""
}

// multiFlag 收集可重复的字符串参数。
type multiFlag []string

func (m *multiFlag) String() string     { return strings.Join(*m, ",") }
func (m *multiFlag) Set(v string) error { *m = append(*m, v); return nil }
