package ctxkeys

import (
	"context"
	"login/data"
	"login/pb/game"
)

// 自定义类型，避免 key 冲突
type contextKey string

const (
	SessionKey        contextKey = "Session"
	SessionIDKey      contextKey = "SessionId"
	SessionDetailsKey contextKey = "SessionDetailsKey"
	// 这里可以继续添加更多 key
)

// 取 session id 的辅助函数
func GetSessionID(ctx context.Context) (string, bool) {
	v := ctx.Value(SessionIDKey)
	s, ok := v.(string)
	return s, ok
}

// 放 session id 的辅助函数
func WithSessionID(ctx context.Context, sessionID string) context.Context {
	return context.WithValue(ctx, SessionIDKey, sessionID)
}

// 取 SessionDetails 的辅助函数
func GetSessionDetails(ctx context.Context) (*game.SessionDetails, bool) {
	v := ctx.Value(SessionDetailsKey)
	detail, ok := v.(*game.SessionDetails)
	return detail, ok
}

// 放 SessionDetails 的辅助函数
func WithSessionDetails(ctx context.Context, detail *game.SessionDetails) context.Context {
	return context.WithValue(ctx, SessionDetailsKey, detail)
}

// 放 Session
func WithSession(ctx context.Context, session *data.Session) context.Context {
	return context.WithValue(ctx, SessionKey, session)
}

// 取 Session
func GetSession(ctx context.Context) (*data.Session, bool) {
	v := ctx.Value(SessionKey)
	session, ok := v.(*data.Session)
	return session, ok
}
