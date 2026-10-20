package serviceauth

import (
	"context"
	"errors"
	"strings"
	"testing"
	"time"

	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
)

func TestControlProofBindsMethodCallerAndWholePayload(t *testing.T) {
	const method = "/mailpb.MailAdmin/SendPersonalMail"
	secret := strings.Repeat("test-only-key-", 4)
	v, err := New(map[string]string{"rank": secret})
	if err != nil {
		t.Fatal(err)
	}
	now := time.Unix(1800000000, 0)
	v.now = func() time.Time { return now }
	// 使用已有带同形状auth的生成请求作为纯签名夹具；资产生产仍用assetop自身签名。
	request := &assetpb.AssetOpRequest{PlayerId: 42, Seq: 1, Bundle: &assetpb.AssetBundle{Items: []*assetpb.ItemGrant{{ConfigId: 7, Count: 3}}}}
	if err := Sign(method, request, "rank", secret, uint64(now.UnixMilli())); err != nil {
		t.Fatal(err)
	}
	if caller, err := v.Verify(context.Background(), method, request); err != nil || caller != "rank" {
		t.Fatal(caller, err)
	}
	for _, mutate := range []func(*assetpb.AssetOpRequest){func(r *assetpb.AssetOpRequest) { r.PlayerId++ }, func(r *assetpb.AssetOpRequest) { r.Bundle.Items[0].Count++ }, func(r *assetpb.AssetOpRequest) { r.Auth.Caller = "ops" }, func(r *assetpb.AssetOpRequest) { r.Auth.TimestampMs++ }} {
		changed := proto.Clone(request).(*assetpb.AssetOpRequest)
		mutate(changed)
		if _, err := v.Verify(context.Background(), method, changed); !errors.Is(err, ErrDenied) {
			t.Fatal("篡改被接受")
		}
	}
	if _, err := v.Verify(context.Background(), "/mailpb.MailAdmin/SendSystemMail", request); !errors.Is(err, ErrDenied) {
		t.Fatal("跨入口重放")
	}
	before, _ := Digest(request)
	if err := Sign(method, request, "rank", secret, uint64(now.Add(time.Second).UnixMilli())); err != nil {
		t.Fatal(err)
	}
	after, _ := Digest(request)
	if before != after {
		t.Fatal("重签不能改变业务幂等摘要")
	}
	v.now = func() time.Time { return now.Add(6 * time.Minute) }
	if _, err := v.Verify(context.Background(), method, request); !errors.Is(err, ErrDenied) {
		t.Fatal("过期证明")
	}
}

func TestControlProofRejectsAbsentProofUnsafeKeysAndCancellation(t *testing.T) {
	if _, err := New(nil); err == nil {
		t.Fatal("空调用方集合")
	}
	if _, err := New(map[string]string{"ops": "short"}); err == nil {
		t.Fatal("短密钥")
	}
	v, _ := New(map[string]string{"ops": strings.Repeat("t", 32)})
	if _, err := v.Verify(context.Background(), "/mailpb.MailAdmin/SendPersonalMail", &assetpb.AssetOpRequest{}); !errors.Is(err, ErrDenied) {
		t.Fatal("缺证明")
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	if _, err := v.Verify(ctx, "/mailpb.MailAdmin/SendPersonalMail", &assetpb.AssetOpRequest{}); !errors.Is(err, context.Canceled) {
		t.Fatal(err)
	}
}
