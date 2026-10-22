package commerce

import (
	"context"
	"database/sql"
	"strconv"

	base "proto/common/base"
	dspb "proto/data_service"
)

// 明确的目录替身仅隔离原commerce状态机夹具，不作为跨域权限或原生冻结运行证据。
type fixtureRoles struct {
	Values map[uint64]*dspb.AccountRoleOwnership
}

func (f fixtureRoles) Ownership(_ context.Context, _ *sql.Tx, player uint64) (*dspb.AccountRoleOwnership, error) {
	if value := f.Values[player]; value != nil {
		return value, nil
	}
	return &dspb.AccountRoleOwnership{Account: "account-" + strconv.FormatUint(player, 10), OwnerRevision: 1, Role: &base.AccountSimplePlayer{PlayerId: player}}, nil
}
func (f fixtureRoles) Lock(ctx context.Context, tx *sql.Tx, listing uint64, players ...uint64) error {
	for _, id := range players {
		if id == 0 {
			continue
		}
		v, _ := f.Ownership(ctx, tx, id)
		if v.TradeListingId != 0 && v.TradeListingId != listing {
			return ErrConflict
		}
	}
	return nil
}
