package admin

import (
	"context"
	"errors"
	"reflect"
	"testing"

	"login/internal/svc"
	base "proto/common/base"
	dspb "proto/data_service"
	"shared/accountdirectory"

	"google.golang.org/protobuf/proto"
)

// SQL 目录替身只模拟已承诺的原子 Remove 接缝；不把它当成真 SQL 锁或提交证据。
type adminDirectoryFake struct {
	roles        map[uint64]*dspb.AccountRoleOwnership
	lookupErrors map[uint64]error
	removeErrors map[string]error
	lookups      []uint64
	removes      map[string][]*dspb.AccountRoleVersion
	beforeRemove func(string, []*dspb.AccountRoleVersion)
}

var _ accountdirectory.Directory = (*adminDirectoryFake)(nil)

func (f *adminDirectoryFake) Read(context.Context, string, bool) (*base.AccountSimplePlayerList, error) {
	panic("孤儿清理不应按账号整块读回再覆盖目录")
}
func (f *adminDirectoryFake) Create(context.Context, string, *base.AccountSimplePlayer) (*base.AccountSimplePlayerList, error) {
	panic("孤儿清理不应创建角色")
}
func (f *adminDirectoryFake) Repair(context.Context, string, *base.AccountSimplePlayer, uint64) (*base.AccountSimplePlayerList, error) {
	panic("孤儿清理不应自愈或回填角色")
}
func (f *adminDirectoryFake) Lookup(_ context.Context, player uint64) (*dspb.AccountRoleOwnership, error) {
	f.lookups = append(f.lookups, player)
	if err := f.lookupErrors[player]; err != nil {
		return nil, err
	}
	role := f.roles[player]
	if role == nil {
		return nil, accountdirectory.ErrMissing
	}
	return proto.Clone(role).(*dspb.AccountRoleOwnership), nil
}
func (f *adminDirectoryFake) Remove(_ context.Context, account string, versions []*dspb.AccountRoleVersion) error {
	if f.removes == nil {
		f.removes = make(map[string][]*dspb.AccountRoleVersion)
	}
	for _, version := range versions {
		f.removes[account] = append(f.removes[account], proto.Clone(version).(*dspb.AccountRoleVersion))
	}
	if f.beforeRemove != nil {
		f.beforeRemove(account, versions)
	}
	if err := f.removeErrors[account]; err != nil {
		return err
	}
	// 先全部复核，再整体提交，故障或归属变化不得部分删除同账号的其余角色。
	for _, version := range versions {
		if role := f.roles[version.PlayerId]; role != nil && (role.Account != account || role.OwnerRevision != version.OwnerRevision) {
			return accountdirectory.ErrConflict
		}
	}
	for _, version := range versions {
		delete(f.roles, version.PlayerId)
	}
	return nil
}

func adminOwned(account string, player, revision uint64, appearance string) *dspb.AccountRoleOwnership {
	return &dspb.AccountRoleOwnership{
		Account: account, OwnerRevision: revision,
		Role: &base.AccountSimplePlayer{PlayerId: player, AppearanceId: appearance},
	}
}

func TestRemovePlayersFromAccounts_SQLDirectoryPreservesRemainingIdentityAndReplay(t *testing.T) {
	kept := adminOwned("admin-retention", 2, 11, "06_thunder_caster_boy")
	directory := &adminDirectoryFake{roles: map[uint64]*dspb.AccountRoleOwnership{
		1: adminOwned("admin-retention", 1, 7, "04_mountain_guardian_boy"),
		2: proto.Clone(kept).(*dspb.AccountRoleOwnership),
	}}
	// Redis 留空，入口必须只依赖全局 SQL 目录，不再读旧反向键或申请 Redis 账号锁。
	sc := &svc.ServiceContext{AccountDirectory: directory}
	out, err := RemovePlayersFromAccounts(context.Background(), sc, &RemovePlayersFromAccountsReq{PlayerIDs: []uint64{1, 1}})
	if err != nil || out == nil || out.RemovedCount != 1 || out.NotFoundCount != 0 || out.FailedCount != 0 {
		t.Fatalf("首次清理计数错误: out=%+v err=%v", out, err)
	}
	if !reflect.DeepEqual(directory.lookups, []uint64{1}) || directory.roles[1] != nil || !proto.Equal(directory.roles[2], kept) {
		t.Fatalf("去重或剩余角色身份错误: lookups=%v roles=%v", directory.lookups, directory.roles)
	}
	versions := directory.removes["admin-retention"]
	if len(versions) != 1 || versions[0].PlayerId != 1 || versions[0].OwnerRevision != 7 {
		t.Fatalf("提交必须带 Lookup 返回的原归属修订: %v", versions)
	}
	out, err = RemovePlayersFromAccounts(context.Background(), sc, &RemovePlayersFromAccountsReq{PlayerIDs: []uint64{1}})
	if err != nil || out.RemovedCount != 0 || out.NotFoundCount != 1 || out.FailedCount != 0 || len(directory.removes["admin-retention"]) != 1 {
		t.Fatalf("已移除角色重放不能复活或再次提交: out=%+v err=%v removes=%v", out, err, directory.removes)
	}
	if !proto.Equal(directory.roles[2], kept) {
		t.Fatal("重放改变了同账号保留角色的外观或归属")
	}
}

func TestRemovePlayersFromAccounts_SQLDirectoryCountsPerOutcomeAndContinuesOtherAccounts(t *testing.T) {
	for _, removeFailure := range []error{accountdirectory.ErrConflict, accountdirectory.ErrMissing, errors.New("SQL结果未知")} {
		t.Run(removeFailure.Error(), func(t *testing.T) {
			directory := &adminDirectoryFake{
				roles: map[uint64]*dspb.AccountRoleOwnership{
					1: adminOwned("acct-free", 1, 3, "甲"),
					2: adminOwned("acct-free", 2, 4, "乙"),
					5: adminOwned("acct-blocked", 5, 8, "丙"),
					6: adminOwned("acct-blocked", 6, 9, "丁"),
				},
				lookupErrors: map[uint64]error{9: errors.New("归属查询依赖失败")},
				removeErrors: map[string]error{"acct-blocked": removeFailure},
			}
			out, err := RemovePlayersFromAccounts(context.Background(), &svc.ServiceContext{AccountDirectory: directory},
				&RemovePlayersFromAccountsReq{PlayerIDs: []uint64{1, 2, 5, 6, 9, 10, 1}})
			if err != nil || out.RemovedCount != 2 || out.FailedCount != 3 || out.NotFoundCount != 1 {
				t.Fatalf("各结局计数错误: out=%+v err=%v", out, err)
			}
			if directory.roles[1] != nil || directory.roles[2] != nil || directory.roles[5] == nil || directory.roles[6] == nil {
				t.Fatalf("组内失败须整体保留、另一账号仍可提交: roles=%v", directory.roles)
			}
			if len(directory.removes["acct-free"]) != 2 || len(directory.removes["acct-blocked"]) != 2 {
				t.Fatalf("应按账号提交各自带修订的去重批次: %v", directory.removes)
			}
			if !reflect.DeepEqual(directory.lookups, []uint64{1, 2, 5, 6, 9, 10}) {
				t.Fatalf("缺失或失败查询不能重试、重复号不能重复计数: %v", directory.lookups)
			}
		})
	}
}

func TestRemovePlayersFromAccounts_OwnerChangeAfterLookupRetainsWholeAccount(t *testing.T) {
	kept := adminOwned("seller-account", 2, 8, "06_thunder_caster_boy")
	directory := &adminDirectoryFake{roles: map[uint64]*dspb.AccountRoleOwnership{
		1: adminOwned("seller-account", 1, 4, "04_mountain_guardian_boy"),
		2: proto.Clone(kept).(*dspb.AccountRoleOwnership),
	}}
	directory.beforeRemove = func(account string, versions []*dspb.AccountRoleVersion) {
		if account != "seller-account" || len(versions) != 2 || versions[0].OwnerRevision != 4 || versions[1].OwnerRevision != 8 {
			t.Fatalf("没有携带原 SQL 归属修订: account=%q versions=%v", account, versions)
		}
		// 确定地插入 Lookup 之后、SQL 提交复核之前的过户，不依赖线程调度或时间等待。
		directory.roles[1] = adminOwned("buyer-account", 1, 5, "04_mountain_guardian_boy")
	}
	out, err := RemovePlayersFromAccounts(context.Background(), &svc.ServiceContext{AccountDirectory: directory},
		&RemovePlayersFromAccountsReq{PlayerIDs: []uint64{1, 2}})
	if err != nil || out.RemovedCount != 0 || out.NotFoundCount != 0 || out.FailedCount != 2 {
		t.Fatalf("归属变化不能算清理成功: out=%+v err=%v", out, err)
	}
	if directory.roles[1].Account != "buyer-account" || directory.roles[1].OwnerRevision != 5 || !proto.Equal(directory.roles[2], kept) {
		t.Fatalf("不得删除新归属或同组未变角色: roles=%v", directory.roles)
	}
	if !reflect.DeepEqual(directory.lookups, []uint64{1, 2}) {
		t.Fatalf("冲突后不能按最新归属偷偷重试删除: %v", directory.lookups)
	}
}

func TestRemovePlayersFromAccounts_RequiresReadyDirectoryAndBoundedRequest(t *testing.T) {
	out, err := RemovePlayersFromAccounts(context.Background(), &svc.ServiceContext{}, &RemovePlayersFromAccountsReq{PlayerIDs: []uint64{1}})
	if out == nil || !errors.Is(err, accountdirectory.ErrUnavailable) {
		t.Fatalf("目录未就绪必须明确失败: out=%+v err=%v", out, err)
	}
	directory := &adminDirectoryFake{}
	sc := &svc.ServiceContext{AccountDirectory: directory}
	for _, req := range []*RemovePlayersFromAccountsReq{nil, {PlayerIDs: make([]uint64, 10001)}} {
		out, err = RemovePlayersFromAccounts(context.Background(), sc, req)
		if out == nil || !errors.Is(err, accountdirectory.ErrInvalid) || len(directory.lookups) != 0 || len(directory.removes) != 0 {
			t.Fatalf("非法或越界请求不得访问目录: out=%+v err=%v lookups=%v removes=%v", out, err, directory.lookups, directory.removes)
		}
	}
	out, err = RemovePlayersFromAccounts(context.Background(), sc, &RemovePlayersFromAccountsReq{})
	if err != nil || out == nil || *out != (RemovePlayersFromAccountsResp{}) || len(directory.lookups) != 0 || len(directory.removes) != 0 {
		t.Fatalf("空请求应无副作用: out=%+v err=%v", out, err)
	}
}
