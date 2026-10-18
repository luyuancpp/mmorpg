package main

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sync/atomic"
	"testing"
	"time"

	"proto/scene"
	"robot/config"

	"gopkg.in/yaml.v3"
)

func assetArrivalTestConfig(t *testing.T) *config.Config {
	t.Helper()
	return &config.Config{
		Mode: "asset-arrival-prepare", GatewayAddr: "http://127.0.0.1:28081", ZoneID: 17101,
		RobotCount: 1, AccountFmt: "unused_%04d", Password: "本地测试占位", AuthType: "password",
		AssetArrivalPrepare: config.AssetArrivalPrepareConfig{
			RunMode: "dev", Dedicated: true, Account: "robot_assetarrival_seller_01",
			GoldTarget: 100, Equipment: true, HoldSeconds: 600,
			OutputPath: filepath.Join(t.TempDir(), "prepared.json"),
		},
	}
}

func TestAssetArrivalPrepareRejectsUnsafeConfiguration(t *testing.T) {
	cases := []struct {
		name string
		edit func(*config.Config)
	}{
		{"生产模式", func(c *config.Config) { c.AssetArrivalPrepare.RunMode = "prod" }},
		{"未确认专用", func(c *config.Config) { c.AssetArrivalPrepare.Dedicated = false }},
		{"普通账号", func(c *config.Config) { c.AssetArrivalPrepare.Account = "robot_9104" }},
		{"账号含格式串", func(c *config.Config) { c.AssetArrivalPrepare.Account = "robot_assetarrival_%04d" }},
		{"账号空后缀", func(c *config.Config) { c.AssetArrivalPrepare.Account = "robot_assetarrival_" }},
		{"远端网关", func(c *config.Config) { c.GatewayAddr = "http://192.0.2.1:28081" }},
		{"域名网关", func(c *config.Config) { c.GatewayAddr = "http://localhost:28081" }},
		{"网关带凭据", func(c *config.Config) { c.GatewayAddr = "http://test@127.0.0.1:28081" }},
		{"网关尾斜杠", func(c *config.Config) { c.GatewayAddr += "/" }},
		{"网关含路径", func(c *config.Config) { c.GatewayAddr += "/proxy" }},
		{"未指定区", func(c *config.Config) { c.ZoneID = 0 }},
		{"超出区库家族", func(c *config.Config) { c.ZoneID = 1000000 }},
		{"多角色", func(c *config.Config) { c.RobotCount = 2 }},
		{"金币不足成交", func(c *config.Config) { c.AssetArrivalPrepare.GoldTarget = 16 }},
		{"无界发钱", func(c *config.Config) { c.AssetArrivalPrepare.GoldTarget = 100001 }},
		{"无保活预算", func(c *config.Config) { c.AssetArrivalPrepare.HoldSeconds = 0 }},
		{"过长保活", func(c *config.Config) { c.AssetArrivalPrepare.HoldSeconds = 1801 }},
		{"相对输出路径", func(c *config.Config) { c.AssetArrivalPrepare.OutputPath = "prepared.json" }},
		{"其他认证", func(c *config.Config) { c.AuthType = "satoken" }},
		{"HTTP认证旁路", func(c *config.Config) { c.UseHttpLogin = true }},
		{"空开发口令", func(c *config.Config) { c.Password = " " }},
		{"AI旁路", func(c *config.Config) { c.LLM.Enabled = true }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			cfg := assetArrivalTestConfig(t)
			tc.edit(cfg)
			if err := cfg.ValidateAssetArrivalPrepare(); err == nil {
				t.Fatal("不安全的准备配置必须在登录和资产副作用前拒绝")
			}
		})
	}
}

func TestAssetArrivalPrepareDoesNotFollowGatewayRedirects(t *testing.T) {
	var redirected atomic.Bool
	target := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		redirected.Store(true)
		_, _ = w.Write([]byte(`{"code":0,"gate_ip":"127.0.0.1","gate_port":28100}`))
	}))
	defer target.Close()
	gateway := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, target.URL, http.StatusTemporaryRedirect)
	}))
	defer gateway.Close()
	if _, err := assignAssetArrivalGate(gateway.URL, &httpAssignGateRequest{ZoneID: 17101}, time.Second); err == nil {
		t.Fatal("本机准备不能接受网关重定向")
	}
	if redirected.Load() {
		t.Fatal("重定向目标不得收到专用准备请求")
	}
}

func TestAssetArrivalPrepareTokenWaitStopsOnCancellation(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	started := make(chan struct{})
	release := make(chan struct{})
	defer close(release)
	done := make(chan error, 1)
	go func() {
		done <- waitAssetArrivalVerification(ctx, func() error {
			close(started)
			<-release
			return nil
		})
	}()
	select {
	case <-started:
	case <-time.After(time.Second):
		t.Fatal("验证等待没有启动")
	}
	cancel()
	select {
	case err := <-done:
		if !errors.Is(err, context.Canceled) {
			t.Fatalf("取消不得伪装成验证成功: %v", err)
		}
	case <-time.After(time.Second):
		t.Fatal("验证回包缺失时不得永久等待")
	}
}

func TestAssetArrivalPrepareConfigRoundTrip(t *testing.T) {
	for _, equipment := range []bool{false, true} {
		cfg := assetArrivalTestConfig(t)
		cfg.AssetArrivalPrepare.Equipment = equipment
		raw, err := yaml.Marshal(cfg)
		if err != nil {
			t.Fatal(err)
		}
		path := filepath.Join(t.TempDir(), "prepare.yaml")
		if err := os.WriteFile(path, raw, 0600); err != nil {
			t.Fatal(err)
		}
		got, err := config.Load(path)
		if err != nil || got == nil || got.AssetArrivalPrepare != cfg.AssetArrivalPrepare {
			t.Fatalf("正式配置装配丢失显式准备选项: %v", err)
		}
	}
}

func TestAssetArrivalPrepareNeverOverwritesExistingOutput(t *testing.T) {
	cfg := assetArrivalTestConfig(t)
	const original = "已有运行结果"
	if err := os.WriteFile(cfg.AssetArrivalPrepare.OutputPath, []byte(original), 0600); err != nil {
		t.Fatal(err)
	}
	// 必须在任何连接、建角或 GM 前拒绝，nil stats 也不会被使用。
	if err := RunAssetArrivalPrepare(cfg, nil); err == nil {
		t.Fatal("不得覆盖现有输出文件")
	}
	raw, err := os.ReadFile(cfg.AssetArrivalPrepare.OutputPath)
	if err != nil || string(raw) != original {
		t.Fatalf("现有结果被修改: %v", err)
	}
}

func TestAssetArrivalPrepareRejectsRemoteAssignment(t *testing.T) {
	for _, reply := range []*httpAssignGateResponse{
		nil, {Code: 100, GateIP: "127.0.0.1", GatePort: 28100},
		{GateIP: "192.0.2.2", GatePort: 28100}, {GateIP: "localhost", GatePort: 28100},
		{GateIP: "127.0.0.1", GatePort: 0}, {GateIP: "127.0.0.1", GatePort: 65536},
	} {
		if err := validateAssetArrivalAssignment(reply); err == nil {
			t.Fatal("非回环、排队或无效端口分配不得进入正式登录")
		}
	}
	if err := validateAssetArrivalAssignment(&httpAssignGateResponse{GateIP: "127.0.0.1", GatePort: 28100}); err != nil {
		t.Fatal(err)
	}
}

func TestAssetArrivalPrepareTargetRequiresOneOriginalEquipment(t *testing.T) {
	weapon := &scene.BagItemInfo{ItemId: ^uint64(0), ConfigId: 1101, Count: 1, MaxStack: 1, EquipKind: 1}
	bags := equipForgeBags{inventory: &scene.BagInfo{Items: []*scene.BagItemInfo{weapon}}, equipment: &scene.BagInfo{}}
	item, worn, err := assetArrivalTarget(bags)
	if err != nil || item != weapon || worn {
		t.Fatalf("有效原件身份丢失: worn=%t err=%v", worn, err)
	}
	bags.equipment.Items = []*scene.BagItemInfo{weapon}
	if _, _, err := assetArrivalTarget(bags); err == nil {
		t.Fatal("同一GUID在两个包或多件候选时不能猜测原件")
	}
	bags.inventory.Items = nil
	if _, worn, err := assetArrivalTarget(bags); err != nil || !worn {
		t.Fatalf("穿戴中的原件应通过正式卸装返回主包: %v", err)
	}
	weapon.Count = 2
	if _, _, err := assetArrivalTarget(bags); err == nil {
		t.Fatal("非单件装备不能作为原件交易输入")
	}
}

func TestAssetArrivalPrepareJSONContainsOnlySelectedIdentifiers(t *testing.T) {
	raw, err := json.Marshal(assetArrivalPrepared{PlayerID: 7, ItemGUID: ^uint64(0), ZoneID: 17101})
	if err != nil {
		t.Fatal(err)
	}
	var fields map[string]json.RawMessage
	if err := json.Unmarshal(raw, &fields); err != nil || len(fields) != 3 ||
		fields["player_id"] == nil || fields["item_guid"] == nil || fields["zone_id"] == nil {
		t.Fatalf("准备凭据必须只包含三个显式标识: %s err=%v", raw, err)
	}
}
