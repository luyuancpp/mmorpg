package ctxkeys

import (
	"context"
	"generated/pb/game/"
)

// 自定义类型，避免 key 冲突
type contextKey string

const (
	SessionDetailsKey contextKey = "SessionDetailsKey"
	// 这里可以继续添加更多 key
)

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
