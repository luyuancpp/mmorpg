package logic

// attachments.go —— 附件的校验、存储编码与发件请求摘要(设计 §2.4、§3.5)。
//
// # 附件存什么
//
// 协议用 mail 自有的 MailCurrency / MailItem(客户端不收 common/asset,§11.1 第 3 条);库里存序列化的 AssetBundle
// (只填 currencies / items),M2 领取时原样作为 SYSTEM_CREDIT 的 bundle —— 发件时校验过的就是将来入账的,同一份字节。
// 两种形状之间**只有**本文件这一处转换;逐字段对齐由 attachments_test.go 对两边的 descriptor 断言守住
// (通道侧加字段时测试变红,提醒这里要跟)。
//
// # 为什么写库前排序
//
// 附件列表的顺序没有业务含义(mail.proto 注释)。按 currency_type / config_id 排序后再编码,
// 同一份附件不论调用方以什么顺序给,存下来的字节与请求摘要都相同 —— 重放不会因为顺序变了而被判成冲突。

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"sort"

	"google.golang.org/protobuf/proto"

	assetpb "proto/common/asset"
	mailpb "proto/mail"
)

// 附件容量 = scene 单个 Credit 包的上限,做到"一封邮件 = 一条 op"(§3.5)。
const (
	maxAttachmentCurrencies = 4
	maxAttachmentItems      = 16
)

// errAttachmentInvalid 是附件形状不合法的统一原因;具体哪条不合法只进日志(发件方是运维 / 机器,看日志即可)。
var errAttachmentInvalid = errors.New("attachments invalid")

// hasAttachments 报告是否带任何附件条目。
func hasAttachments(a *mailpb.MailAttachments) bool {
	return len(a.GetCurrencies()) > 0 || len(a.GetItems()) > 0
}

// normalizeAttachments 校验附件并返回排序后的副本(nil = 没有附件)。规则(§3.5):
// 货币 ≤ 4 条、currency_type 不重复且在白名单内、1 ≤ amount ≤ INT64_MAX;物品 ≤ 16 条、config_id 非 0 且不重复、count ≥ 1。
// ⚠ config_id 是否存在于 Item 表 M1 校验不了(mail 不加载配置表);M1 生产不许发附件(M-9),缺口只影响 dev。
func normalizeAttachments(a *mailpb.MailAttachments, allowCurrency func(uint32) bool) (*mailpb.MailAttachments, error) {
	if !hasAttachments(a) {
		return nil, nil
	}
	currencies := a.GetCurrencies()
	items := a.GetItems()
	if len(currencies) > maxAttachmentCurrencies {
		return nil, fmt.Errorf("%w: %d currencies (max %d)", errAttachmentInvalid, len(currencies), maxAttachmentCurrencies)
	}
	if len(items) > maxAttachmentItems {
		return nil, fmt.Errorf("%w: %d items (max %d)", errAttachmentInvalid, len(items), maxAttachmentItems)
	}
	out := &mailpb.MailAttachments{
		Currencies: make([]*mailpb.MailCurrency, 0, len(currencies)),
		Items:      make([]*mailpb.MailItem, 0, len(items)),
	}
	seenCurrency := make(map[uint32]struct{}, len(currencies))
	for _, c := range currencies {
		if c == nil {
			return nil, fmt.Errorf("%w: nil currency entry", errAttachmentInvalid)
		}
		if !allowCurrency(c.GetCurrencyType()) {
			return nil, fmt.Errorf("%w: currency_type %d not in Mail.AllowedCurrencyTypes", errAttachmentInvalid, c.GetCurrencyType())
		}
		if _, dup := seenCurrency[c.GetCurrencyType()]; dup {
			return nil, fmt.Errorf("%w: duplicate currency_type %d", errAttachmentInvalid, c.GetCurrencyType())
		}
		seenCurrency[c.GetCurrencyType()] = struct{}{}
		if c.GetAmount() == 0 || c.GetAmount() > math.MaxInt64 {
			return nil, fmt.Errorf("%w: currency_type %d amount %d out of [1, INT64_MAX]", errAttachmentInvalid, c.GetCurrencyType(), c.GetAmount())
		}
		out.Currencies = append(out.Currencies, &mailpb.MailCurrency{CurrencyType: c.GetCurrencyType(), Amount: c.GetAmount()})
	}
	seenItem := make(map[uint32]struct{}, len(items))
	for _, it := range items {
		if it == nil {
			return nil, fmt.Errorf("%w: nil item entry", errAttachmentInvalid)
		}
		if it.GetConfigId() == 0 {
			return nil, fmt.Errorf("%w: item config_id 0", errAttachmentInvalid)
		}
		if _, dup := seenItem[it.GetConfigId()]; dup {
			return nil, fmt.Errorf("%w: duplicate item config_id %d", errAttachmentInvalid, it.GetConfigId())
		}
		seenItem[it.GetConfigId()] = struct{}{}
		if it.GetCount() == 0 {
			return nil, fmt.Errorf("%w: item config_id %d count 0", errAttachmentInvalid, it.GetConfigId())
		}
		out.Items = append(out.Items, &mailpb.MailItem{ConfigId: it.GetConfigId(), Count: it.GetCount()})
	}
	sort.Slice(out.Currencies, func(i, j int) bool { return out.Currencies[i].GetCurrencyType() < out.Currencies[j].GetCurrencyType() })
	sort.Slice(out.Items, func(i, j int) bool { return out.Items[i].GetConfigId() < out.Items[j].GetConfigId() })
	return out, nil
}

// toAssetBundle 是 MailAttachments → AssetBundle 的**唯一**转换(item_uuids / pet_id 永远留空:
// scene 的 ValidateCreditBundle 见到它们就终局拒,§3.5)。
func toAssetBundle(a *mailpb.MailAttachments) *assetpb.AssetBundle {
	bundle := &assetpb.AssetBundle{}
	for _, c := range a.GetCurrencies() {
		bundle.Currencies = append(bundle.Currencies, &assetpb.CurrencyAmount{CurrencyType: c.GetCurrencyType(), Amount: c.GetAmount()})
	}
	for _, it := range a.GetItems() {
		bundle.Items = append(bundle.Items, &assetpb.ItemGrant{ConfigId: it.GetConfigId(), Count: it.GetCount()})
	}
	return bundle
}

// encodeAttachments 把(已 normalize 的)附件编码成库里的 attachments 列。没有附件 = nil(列里是空串,claim_state 推导为 0)。
func encodeAttachments(a *mailpb.MailAttachments) ([]byte, error) {
	if !hasAttachments(a) {
		return nil, nil
	}
	b, err := proto.MarshalOptions{Deterministic: true}.Marshal(toAssetBundle(a))
	if err != nil {
		return nil, fmt.Errorf("marshal attachments: %w", err)
	}
	return b, nil
}

// decodeAttachments 把库里的 attachments 列解回客户端形状。空 = 没有附件(nil)。
// 解出 item_uuids / pet_id 视为损坏:mail 从不写这两个字段,出现了只能是行被别的东西改写过。
func decodeAttachments(b []byte) (*mailpb.MailAttachments, error) {
	if len(b) == 0 {
		return nil, nil
	}
	bundle := &assetpb.AssetBundle{}
	if err := proto.Unmarshal(b, bundle); err != nil {
		return nil, fmt.Errorf("decode attachments: %w", err)
	}
	if len(bundle.GetItemUuids()) > 0 || bundle.GetPetId() != 0 {
		return nil, errors.New("decode attachments: unexpected item_uuids / pet_id in mail attachments")
	}
	out := &mailpb.MailAttachments{}
	for _, c := range bundle.GetCurrencies() {
		out.Currencies = append(out.Currencies, &mailpb.MailCurrency{CurrencyType: c.GetCurrencyType(), Amount: c.GetAmount()})
	}
	for _, it := range bundle.GetItems() {
		out.Items = append(out.Items, &mailpb.MailItem{ConfigId: it.GetConfigId(), Count: it.GetCount()})
	}
	return out, nil
}

// ════════════════════════════════════════════════════════════════════════════
// 请求摘要
// ════════════════════════════════════════════════════════════════════════════

// digestVersion 是摘要编码的版本前缀。摘要存在库里、跨版本比较:编码规则一旦改动,已存的摘要与新算的就对不上,
// 重放会被误判成 MailRequestConflict。所以这里**手写**定长 / 带长度前缀的编码,不用 proto 序列化
// (proto 的 Deterministic 只保证同一个二进制内稳定,不保证跨版本);改规则必须换版本前缀并评估存量行。
const digestVersion = "mail.request.v1"

// 摘要里区分两种发件,避免同一组字段在两种方法之间巧合相等(两张表的幂等键本就不同,这只是多一道保险)。
const (
	digestKindPersonal byte = 'P'
	digestKindSystem   byte = 'S'
)

// sendDigestInput 是参与摘要的请求内容。sent_ms 不在里面:它是服务端时刻,重放时必然不同。
type sendDigestInput struct {
	kind        byte
	senderKind  uint32
	requestID   uint64
	recipientID uint64 // 系统邮件为 0
	senderName  string
	title       string
	body        string
	attachments *mailpb.MailAttachments // 已 normalize(排序)
	expireMs    uint64
}

// requestDigest 对规范化后的请求算 SHA-256(§2.4)。
func requestDigest(in sendDigestInput) []byte {
	var buf bytes.Buffer
	writeString(&buf, digestVersion)
	buf.WriteByte(in.kind)
	writeUint64(&buf, uint64(in.senderKind))
	writeUint64(&buf, in.requestID)
	writeUint64(&buf, in.recipientID)
	writeString(&buf, in.senderName)
	writeString(&buf, in.title)
	writeString(&buf, in.body)
	currencies := in.attachments.GetCurrencies()
	writeUint64(&buf, uint64(len(currencies)))
	for _, c := range currencies {
		writeUint64(&buf, uint64(c.GetCurrencyType()))
		writeUint64(&buf, c.GetAmount())
	}
	items := in.attachments.GetItems()
	writeUint64(&buf, uint64(len(items)))
	for _, it := range items {
		writeUint64(&buf, uint64(it.GetConfigId()))
		writeUint64(&buf, uint64(it.GetCount()))
	}
	writeUint64(&buf, in.expireMs)
	sum := sha256.Sum256(buf.Bytes())
	return sum[:]
}

func writeUint64(buf *bytes.Buffer, v uint64) {
	var b [8]byte
	binary.BigEndian.PutUint64(b[:], v)
	buf.Write(b[:])
}

// writeString 写长度前缀 + 字节:没有前缀时 ("ab","c") 与 ("a","bc") 拼出同一串。
func writeString(buf *bytes.Buffer, s string) {
	writeUint64(buf, uint64(len(s)))
	buf.WriteString(s)
}
