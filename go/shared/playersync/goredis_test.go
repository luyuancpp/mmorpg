package playersync

// goredis_test.go —— go-redis 适配器的 nil 句柄形态(NewGoRedisEvaler 的文档承诺)。
// 无类型 nil 的用例在 digest_test.go 的 TestNilBumperIsNoop 里;这里只管带类型的 nil 指针。

import (
	"testing"

	goredis "github.com/redis/go-redis/v9"
)

// 带类型的 nil 指针装进 UniversalClient 接口之后不等于 nil:适配器必须照样返回 nil,
// 否则 NewBumper 拿到的是一个非 nil 的 Bumper,第一次 Eval 在调用方异步的 bump 协程里空指针 panic。
func TestGoRedisEvalerTypedNilIsNil(t *testing.T) {
	cases := []struct {
		name string
		c    goredis.UniversalClient
	}{
		{"*Client", (*goredis.Client)(nil)},
		{"*ClusterClient", (*goredis.ClusterClient)(nil)},
		{"*Ring", (*goredis.Ring)(nil)},
	}
	for _, tc := range cases {
		if NewGoRedisEvaler(tc.c) != nil {
			t.Fatalf("NewGoRedisEvaler((%s)(nil)) 应返回 nil Evaler", tc.name)
		}
		if NewBumper(NewGoRedisEvaler(tc.c), "x") != nil {
			t.Fatalf("(%s)(nil) 句柄时 Bumper 应为 nil", tc.name)
		}
	}
}
