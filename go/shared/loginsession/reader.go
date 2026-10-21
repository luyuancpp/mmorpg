// Package loginsession 定义登录态键的共用拼法及只读接缝；不拥有登录凭据或写权限。
package loginsession

import (
	"context"
	"errors"
	"reflect"
	"strconv"

	base "proto/common/base"
	"shared/accountdirectory"

	"github.com/redis/go-redis/v9"
)

// KeyFromMember 也供login原Ref使用，避免不同服务维护第二份键规则。
func KeyFromMember(member string) string { return "login_session:" + member }

// ConnectionKey 仅受理具有进程UUID与单调代次的gate连接。资产/人民币路径不继承旧session_id撞号兼容。
func ConnectionKey(in *base.SessionDetails) (string, error) {
	if in == nil || in.GetSessionId() == 0 || in.GetGateInstanceId() == "" || in.GetSessionIncarnation() == 0 {
		return "", errors.New("登录连接身份不完整")
	}
	return KeyFromMember(in.GateInstanceId + ":" + strconv.FormatUint(in.SessionIncarnation, 10)), nil
}

type Reader interface {
	Get(context.Context, string) *redis.StringCmd
}

// Account 必须读取部署显式指定的login登录态实例；调用者不能传入要查询的账号。
func Account(ctx context.Context, r Reader, in *base.SessionDetails) (string, error) {
	key, err := ConnectionKey(in)
	if err != nil || r == nil {
		return "", errors.New("登录态依赖或连接身份不可用")
	}
	value:=reflect.ValueOf(r)
	switch value.Kind() {
	case reflect.Pointer,reflect.Interface,reflect.Map,reflect.Chan,reflect.Func,reflect.Slice:
		if value.IsNil(){return "",errors.New("登录态只读连接未装配")}
	}
	account, err := r.Get(ctx, key).Result()
	if err != nil {
		return "", err
	}
	if !accountdirectory.ValidAccount(account) {
		return "", errors.New("登录态账号非法")
	}
	return account, nil
}
