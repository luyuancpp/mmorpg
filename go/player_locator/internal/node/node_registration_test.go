package node

import (
	"testing"

	clientv3 "go.etcd.io/etcd/client/v3"
)

// 能力键等标记按「前缀 + NodeUuid」展开成 Put,值为 "1",顺序与前缀一致;不给前缀时不产生任何 Op,
// 注册事务退化为原来的两键(与其它服务的 allocator 副本同义)。租约绑定在 integration 用例里核对。
func TestLeaseBoundMarkerOps(t *testing.T) {
	const uuid = "3f0c6a52-6d1e-4b7a-9a41-0c2f7e5b8d10"
	lease := clientv3.LeaseID(42)

	if ops := leaseBoundMarkerOps(nil, uuid, lease); len(ops) != 0 {
		t.Fatalf("无标记时应不产生 Op,得到 %d 个", len(ops))
	}

	prefixes := []string{"/mmorpg/capability/a/v1/", "/mmorpg/capability/b/v1/"}
	ops := leaseBoundMarkerOps(prefixes, uuid, lease)
	if len(ops) != len(prefixes) {
		t.Fatalf("Op 数 = %d, want %d", len(ops), len(prefixes))
	}
	for i, op := range ops {
		if !op.IsPut() {
			t.Errorf("ops[%d] 必须是 Put", i)
		}
		if got, want := string(op.KeyBytes()), prefixes[i]+uuid; got != want {
			t.Errorf("ops[%d] key = %q, want %q", i, got, want)
		}
		if got := string(op.ValueBytes()); got != "1" {
			t.Errorf("ops[%d] value = %q, want \"1\"", i, got)
		}
	}
}

// WithLeaseBoundMarkers 可叠加,NewNode 按传入顺序收集前缀。
func TestWithLeaseBoundMarkersAccumulates(t *testing.T) {
	var o nodeOptions
	for _, opt := range []Option{WithLeaseBoundMarkers("/a/"), WithLeaseBoundMarkers("/b/", "/c/")} {
		opt(&o)
	}
	want := []string{"/a/", "/b/", "/c/"}
	if len(o.markerPrefixes) != len(want) {
		t.Fatalf("markerPrefixes = %v, want %v", o.markerPrefixes, want)
	}
	for i := range want {
		if o.markerPrefixes[i] != want[i] {
			t.Fatalf("markerPrefixes = %v, want %v", o.markerPrefixes, want)
		}
	}
}
