package loginsessionstore

import (
	"context"
	"fmt"
	login_proto "login/proto/service/go/grpc/login"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

func sessionKey(sessionId uint64) string {
	return fmt.Sprintf("login_session_info:%d", sessionId)
}

// SaveLoginSession 存储登录会话信息到 RedisClient
// 修改后的函数签名：
func SaveLoginSession(ctx context.Context, redisClient *redis.Client, info *login_proto.LoginSessionInfo, ttl time.Duration) error {
	key := sessionKey(info.SessionId)
	data, err := proto.Marshal(info)
	if err != nil {
		return err
	}
	return redisClient.Set(ctx, key, data, ttl).Err()
}

// LoadLoginSession 通过 sessionId 获取会话信息
func GetLoginSession(ctx context.Context, redisClient *redis.Client, sessionId uint64) (*login_proto.LoginSessionInfo, error) {
	key := sessionKey(sessionId)
	val, err := redisClient.Get(ctx, key).Bytes()
	if err != nil {
		return nil, err
	}
	info := &login_proto.LoginSessionInfo{}
	if err := proto.Unmarshal(val, info); err != nil {
		return nil, err
	}
	return info, nil
}

// DeleteLoginSession 删除登录会话
func DeleteLoginSession(ctx context.Context, redisClient *redis.Client, sessionId uint64) error {
	return redisClient.Del(ctx, sessionKey(sessionId)).Err()
}
