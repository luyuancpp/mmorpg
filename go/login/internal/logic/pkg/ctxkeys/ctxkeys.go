package ctxkeys

import (
	"context"
	login_proto "login/proto/common/base"
)

// contextKey is a custom type to avoid key collisions.
type contextKey string

const (
	SessionDetailsKey contextKey = "SessionDetailsKey"
)

// GetSessionDetails retrieves SessionDetails from the context.
func GetSessionDetails(ctx context.Context) (*login_proto.SessionDetails, bool) {
	v := ctx.Value(SessionDetailsKey)
	detail, ok := v.(*login_proto.SessionDetails)
	return detail, ok
}

// WithSessionDetails stores SessionDetails in the context.
func WithSessionDetails(ctx context.Context, detail *login_proto.SessionDetails) context.Context {
	return context.WithValue(ctx, SessionDetailsKey, detail)
}
