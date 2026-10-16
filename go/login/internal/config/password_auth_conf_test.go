package config

import (
	"testing"
	"time"

	"github.com/zeromicro/go-zero/core/conf"
)

func TestPasswordFailureQuotaConfigurationDefaultsAndBounds(t *testing.T) {
	var parsed struct {
		PasswordAuth PasswordAuthConf `json:"PasswordAuth"`
	}
	if err := conf.LoadFromYamlBytes([]byte("PasswordAuth:\n  Enabled: true\n"), &parsed); err != nil {
		t.Fatal(err)
	}
	c := parsed.PasswordAuth
	if c.FailureLimit != 5 || c.FailureWindow != 15*time.Minute || c.FailureLock != 5*time.Minute {
		t.Fatalf("默认配置=%+v", c)
	}
	if err := c.ValidateFailureQuota(); err != nil {
		t.Fatal(err)
	}
	for _, change := range []func(*PasswordAuthConf){func(c *PasswordAuthConf) { c.FailureLimit = 0 }, func(c *PasswordAuthConf) { c.FailureLimit = 101 }, func(c *PasswordAuthConf) { c.FailureWindow = 0 }, func(c *PasswordAuthConf) { c.FailureLock = 2 * time.Hour }, func(c *PasswordAuthConf) { c.FailureWindow = time.Second + time.Nanosecond }} {
		invalid := c
		change(&invalid)
		if err := invalid.ValidateFailureQuota(); err == nil {
			t.Fatal("无效配额配置被接受")
		}
	}
	for _, valid := range []PasswordAuthConf{{FailureLimit: 1, FailureWindow: time.Second, FailureLock: time.Second}, {FailureLimit: 100, FailureWindow: time.Hour, FailureLock: time.Hour}} {
		if err := valid.ValidateFailureQuota(); err != nil {
			t.Fatal(err)
		}
	}
}
