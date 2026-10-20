// Package serviceauth 只签内部邮件/排行榜控制请求，不替代资产通道自身的签名。
package serviceauth

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

var ErrDenied = errors.New("内部调用证明无效")
var callerPattern = regexp.MustCompile(`^[a-z][a-z0-9_-]{0,31}$`)

// Proof 字段通过请求自身的proto descriptor访问，避免并行维护第二份协议struct。
func proof(request proto.Message) (protoreflect.Message, error) {
	if request == nil || !request.ProtoReflect().IsValid() {
		return nil, ErrDenied
	}
	m := request.ProtoReflect()
	f := m.Descriptor().Fields().ByName("auth")
	if f == nil || f.Kind() != protoreflect.MessageKind {
		return nil, ErrDenied
	}
	a := m.Mutable(f).Message()
	fields := a.Descriptor().Fields()
	for name, kind := range map[protoreflect.Name]protoreflect.Kind{"caller": protoreflect.StringKind, "timestamp_ms": protoreflect.Uint64Kind, "signature_hex": protoreflect.StringKind} {
		f := fields.ByName(name)
		if f == nil || f.Kind() != kind {
			return nil, ErrDenied
		}
	}
	return a, nil
}

// Digest 清空auth后取确定性PB摘要；业务幂等同样保存它，不把秘密写入状态。
func Digest(request proto.Message) (string, error) {
	if request == nil || !request.ProtoReflect().IsValid() {
		return "", ErrDenied
	}
	copy := proto.Clone(request)
	m := copy.ProtoReflect()
	if f := m.Descriptor().Fields().ByName("auth"); f != nil {
		m.Clear(f)
	}
	bytes, err := proto.MarshalOptions{Deterministic: true}.Marshal(copy)
	if err != nil {
		return "", ErrDenied
	}
	sum := sha256.Sum256(bytes)
	return hex.EncodeToString(sum[:]), nil
}

func canonical(method, caller string, stamp uint64, digest string) (string, error) {
	if !strings.HasPrefix(method, "/") || strings.ContainsAny(method, "\r\n") || !callerPattern.MatchString(caller) || stamp == 0 {
		return "", ErrDenied
	}
	return "v1\n" + method + "\n" + caller + "\n" + strconv.FormatUint(stamp, 10) + "\n" + digest, nil
}

// Sign 只供已授权生产者装配调用；每次重投保留业务request_id/cursor，只更新证明。
func Sign(method string, request proto.Message, caller, rawSecret string, stamp uint64) error {
	secret := strings.TrimSpace(rawSecret)
	if len([]byte(secret)) < 32 {
		return ErrDenied
	}
	digest, err := Digest(request)
	if err != nil {
		return err
	}
	text, err := canonical(method, caller, stamp, digest)
	if err != nil {
		return err
	}
	a, err := proof(request)
	if err != nil {
		return err
	}
	f := a.Descriptor().Fields()
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write([]byte(text))
	a.Set(f.ByName("caller"), protoreflect.ValueOfString(caller))
	a.Set(f.ByName("timestamp_ms"), protoreflect.ValueOfUint64(stamp))
	a.Set(f.ByName("signature_hex"), protoreflect.ValueOfString(hex.EncodeToString(mac.Sum(nil))))
	return nil
}

// Verifier 固定调用方集合由装配层传入。请求中的caller不用于拼环境变量名或查任意密钥。
type Verifier struct {
	secrets map[string]string
	now     func() time.Time
}

func New(secrets map[string]string) (*Verifier, error) {
	if len(secrets) == 0 {
		return nil, fmt.Errorf("至少配置一个内部生产者")
	}
	copy := make(map[string]string, len(secrets))
	for caller, raw := range secrets {
		secret := strings.TrimSpace(raw)
		if !callerPattern.MatchString(caller) || len([]byte(secret)) < 32 {
			return nil, fmt.Errorf("内部生产者密钥未配置或不足32字节")
		}
		copy[caller] = secret
	}
	return &Verifier{secrets: copy, now: time.Now}, nil
}

// Verify 返回被验证的角色。方法级角色准入仍由真实业务入口明确检查。
func (v *Verifier) Verify(ctx context.Context, method string, request proto.Message) (string, error) {
	if err := ctx.Err(); err != nil {
		return "", err
	}
	if request == nil || !request.ProtoReflect().IsValid() {
		return "", ErrDenied
	}
	clone := proto.Clone(request)
	a, err := proof(clone)
	if err != nil {
		return "", err
	}
	f := a.Descriptor().Fields()
	caller := a.Get(f.ByName("caller")).String()
	secret, ok := v.secrets[caller]
	if !ok {
		return "", ErrDenied
	}
	stamp := a.Get(f.ByName("timestamp_ms")).Uint()
	now := v.now().UnixMilli()
	if now < 0 || stamp > uint64(now)+300000 || uint64(now) > stamp+300000 || stamp > uint64(^uint64(0)>>1) {
		return "", ErrDenied
	}
	signature := a.Get(f.ByName("signature_hex")).String()
	if len(signature) != 64 || signature != strings.ToLower(signature) {
		return "", ErrDenied
	}
	expected, err := hex.DecodeString(signature)
	if err != nil {
		return "", ErrDenied
	}
	digest, err := Digest(request)
	if err != nil {
		return "", err
	}
	text, err := canonical(method, caller, stamp, digest)
	if err != nil {
		return "", err
	}
	mac := hmac.New(sha256.New, []byte(secret))
	_, _ = mac.Write([]byte(text))
	if !hmac.Equal(expected, mac.Sum(nil)) {
		return "", ErrDenied
	}
	if err := ctx.Err(); err != nil {
		return "", err
	}
	return caller, nil
}
