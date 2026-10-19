package main

// equip-forge-topup 的配置校验与「发到了没有」的核对:不连服务器,只喂构造出来的快照。

import (
	"strings"
	"testing"

	"robot/config"
)

func equipForgeTopupTestConfig() config.EquipForgeTopupConfig {
	return config.EquipForgeTopupConfig{
		Account: equipForgeAccount,
		Gold:    1000,
		Items: []config.EquipForgeTopupItem{
			{ConfigId: equipForgeStone, Count: 3},
			{ConfigId: equipForgeGemAttackL1, Count: 1},
		},
	}
}

func TestEquipForgeTopupConfigRejectsWhatWouldFailLater(t *testing.T) {
	ok := equipForgeTopupTestConfig()
	if err := ok.Validate(); err != nil {
		t.Fatalf("valid config rejected: %v", err)
	}
	goldOnly := config.EquipForgeTopupConfig{Account: "robot_1", Gold: 1}
	if err := goldOnly.Validate(); err != nil {
		t.Fatalf("gold-only config rejected: %v", err)
	}
	itemsOnly := config.EquipForgeTopupConfig{Account: "robot_1", Items: []config.EquipForgeTopupItem{{ConfigId: 1, Count: config.EquipForgeTopupMaxCount}}}
	if err := itemsOnly.Validate(); err != nil {
		t.Fatalf("items-only config at the per-call cap rejected: %v", err)
	}

	cases := map[string]func(c *config.EquipForgeTopupConfig){
		"account without the dev prefix": func(c *config.EquipForgeTopupConfig) { c.Account = "player_1" },
		"empty account":                  func(c *config.EquipForgeTopupConfig) { c.Account = "" },
		"negative gold":                  func(c *config.EquipForgeTopupConfig) { c.Gold = -1 },
		"nothing to grant":               func(c *config.EquipForgeTopupConfig) { c.Gold = 0; c.Items = nil },
		"zero config id":                 func(c *config.EquipForgeTopupConfig) { c.Items[1].ConfigId = 0 },
		"zero count":                     func(c *config.EquipForgeTopupConfig) { c.Items[0].Count = 0 },
		"count over the per-call cap":    func(c *config.EquipForgeTopupConfig) { c.Items[0].Count = config.EquipForgeTopupMaxCount + 1 },
	}
	for name, mutate := range cases {
		c := equipForgeTopupTestConfig()
		c.Items = append([]config.EquipForgeTopupItem(nil), c.Items...)
		mutate(&c)
		if err := c.Validate(); err == nil {
			t.Errorf("%s: accepted", name)
		}
	}
}

// 每项上限必须与 GmGrantItem 的单次上限一致,否则配置校验放过的数量会在 equipForgeGrant 里才被拒。
func TestEquipForgeTopupCapMatchesTheGrantHelper(t *testing.T) {
	if uint64(config.EquipForgeTopupMaxCount) != equipForgeGrantMaxCount {
		t.Fatalf("config cap %d != grant cap %d", config.EquipForgeTopupMaxCount, equipForgeGrantMaxCount)
	}
}

// 仓库里那份 yaml 必须能直接用:模式对、账号就是 equip-forge-smoke 那个、数量够从 +0 强化到满级。
func TestEquipForgeTopupShippedYamlLoadsAndCoversAFullEnhanceRun(t *testing.T) {
	cfg, err := config.Load("etc/equip_forge_topup.yaml")
	if err != nil {
		t.Fatalf("load etc/equip_forge_topup.yaml: %v", err)
	}
	if cfg.Mode != "equip-forge-topup" {
		t.Fatalf("mode = %q", cfg.Mode)
	}
	topup := cfg.EquipForgeTopup
	if topup.Account != equipForgeAccount {
		t.Fatalf("account = %q, want the equip-forge-smoke account %q", topup.Account, equipForgeAccount)
	}
	// data/EquipEnhance.xlsx(2026-10):+1 → +12 合计银两 331000、强化石 49 颗。
	want := equipForgeTopupWanted(topup)
	if topup.Gold < 331000 || want[equipForgeStone] < 49 {
		t.Fatalf("defaults too small for one full +0 → +12 run: gold=%d stones=%d", topup.Gold, want[equipForgeStone])
	}
	if want[equipForgeGemAttackL1] == 0 {
		t.Fatal("defaults must include an attack gem (the attack socket accepts only that type)")
	}
}

func TestEquipForgeTopupShortfallsAcceptOnlyAFullTopup(t *testing.T) {
	topup := equipForgeTopupTestConfig()
	before := equipForgeTestBags(equipForgeTestWeapon(equipForgeTestTargetId, 0), false, 500, 2, 1)

	granted := equipForgeTestClone(before)
	equipForgeTestSetCount(granted, equipForgeTestStoneId, 5)
	equipForgeTestSetCount(granted, equipForgeTestGemId, 2)
	equipForgeTestSetGold(granted, 1500)
	if problems := equipForgeTopupShortfalls(before.inventory, granted.inventory, topup); len(problems) != 0 {
		t.Fatalf("full topup reported problems: %v", problems)
	}
	summary := equipForgeTopupSummary(before.inventory, granted.inventory, topup)
	for _, part := range []string{"account=" + equipForgeAccount, "gold=500->1500", "item_2001=2->5", "item_2101=1->2"} {
		if !strings.Contains(summary, part) {
			t.Fatalf("summary %q lacks %q", summary, part)
		}
	}
	if strings.Index(summary, "item_2001") > strings.Index(summary, "item_2101") {
		t.Fatalf("summary not in config order: %q", summary)
	}

	// 多给不算错(同账号别处也在加东西)。
	extra := equipForgeTestClone(granted)
	equipForgeTestSetCount(extra, equipForgeTestStoneId, 50)
	if problems := equipForgeTopupShortfalls(before.inventory, extra.inventory, topup); len(problems) != 0 {
		t.Fatalf("extra items reported problems: %v", problems)
	}

	short := equipForgeTestClone(granted)
	equipForgeTestSetCount(short, equipForgeTestStoneId, 4) // 只多了 2 颗
	if problems := equipForgeTopupShortfalls(before.inventory, short.inventory, topup); len(problems) != 1 ||
		!strings.Contains(problems[0], "config_id=2001") {
		t.Fatalf("short stones not reported exactly: %v", problems)
	}

	noGold := equipForgeTestClone(granted)
	equipForgeTestSetGold(noGold, 500)
	if problems := equipForgeTopupShortfalls(before.inventory, noGold.inventory, topup); len(problems) != 1 ||
		!strings.Contains(problems[0], "银两") {
		t.Fatalf("missing gold not reported exactly: %v", problems)
	}

	// 反而变少了(读到的是更旧的快照,或被别处消耗):也算没发到,不能因为无符号相减绕回去而放过。
	fewer := equipForgeTestClone(before)
	equipForgeTestSetCount(fewer, equipForgeTestStoneId, 1)
	equipForgeTestSetGold(fewer, 100)
	if problems := equipForgeTopupShortfalls(before.inventory, fewer.inventory, topup); len(problems) != 3 {
		t.Fatalf("decreases must be reported for stones, gem and gold: %v", problems)
	}
}

// 同一个 config 写了两项:按合计核对。
func TestEquipForgeTopupSumsRepeatedConfigs(t *testing.T) {
	topup := config.EquipForgeTopupConfig{Account: equipForgeAccount, Items: []config.EquipForgeTopupItem{
		{ConfigId: equipForgeStone, Count: 2}, {ConfigId: equipForgeStone, Count: 3},
	}}
	before := equipForgeTestBags(equipForgeTestWeapon(equipForgeTestTargetId, 0), false, 0, 1, 0)
	after := equipForgeTestClone(before)
	equipForgeTestSetCount(after, equipForgeTestStoneId, 5) // 只多了 4 颗,合计要 5 颗
	if problems := equipForgeTopupShortfalls(before.inventory, after.inventory, topup); len(problems) != 1 {
		t.Fatalf("repeated config not summed: %v", problems)
	}
	equipForgeTestSetCount(after, equipForgeTestStoneId, 6)
	if problems := equipForgeTopupShortfalls(before.inventory, after.inventory, topup); len(problems) != 0 {
		t.Fatalf("summed topup reported problems: %v", problems)
	}
}
