package fsmstore

import (
	"context"
	"testing"

	"github.com/looplab/fsm"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
)

func newTestFSM() *fsm.FSM {
	return fsm.NewFSM(
		"start",
		fsm.Events{
			{Name: "to_middle", Src: []string{"start"}, Dst: "middle"},
			{Name: "to_end", Src: []string{"middle"}, Dst: "end"},
		},
		fsm.Callbacks{},
	)
}

func TestFSMStore_SaveAndLoad(t *testing.T) {
	ctx := context.Background()

	// Create local Redis test connection (default 127.0.0.1:6379)
	rdb := redis.NewClient(&redis.Options{
		Addr: "127.0.0.1:6379",
		DB:   1, // Use test DB to avoid polluting main DB
	})

	account := "testAccount"
	roleId := "role1"
	key := redisKey(account, roleId)

	// Cleanup
	defer func() {
		_ = rdb.Del(ctx, key).Err()
	}()

	// Initialize FSM
	f := newTestFSM()

	// Execute state transition
	err := f.Event(ctx, "to_middle")
	assert.NoError(t, err)
	assert.Equal(t, "middle", f.Current())

	// Save state
	err = SaveFSMState(ctx, rdb, f, account, roleId)
	assert.NoError(t, err)

	// Create new FSM to simulate service restart
	newFSM := newTestFSM()
	assert.Equal(t, "start", newFSM.Current())

	// Restore state
	err = LoadFSMState(ctx, rdb, newFSM, account, roleId)
	assert.NoError(t, err)
	assert.Equal(t, "middle", newFSM.Current())

	// Transition again
	err = newFSM.Event(ctx, "to_end")
	assert.NoError(t, err)
	assert.Equal(t, "end", newFSM.Current())

	// Overwrite with new state
	err = SaveFSMState(ctx, rdb, newFSM, account, roleId)
	assert.NoError(t, err)

	// Verify state in Redis
	val, err := rdb.Get(ctx, key).Result()
	assert.NoError(t, err)
	assert.Equal(t, "end", val)
}
