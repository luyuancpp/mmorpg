package auth

import (
	"context"
	"fmt"
	"sync"
)

// AuthResult holds the resolved account from authentication.
type AuthResult struct {
	Account string // Resolved account identifier
}

// Provider validates an auth token and returns the resolved account.
type Provider interface {
	Validate(ctx context.Context, token string) (*AuthResult, error)
}

var (
	mu        sync.RWMutex
	providers = map[string]Provider{}
)

// Register adds a named auth provider. Panics on duplicate names.
func Register(name string, p Provider) {
	mu.Lock()
	defer mu.Unlock()
	if _, exists := providers[name]; exists {
		panic(fmt.Sprintf("auth: provider %q already registered", name))
	}
	providers[name] = p
}

// Get returns the provider for the given auth type, or nil if not registered.
func Get(name string) Provider {
	mu.RLock()
	defer mu.RUnlock()
	return providers[name]
}
