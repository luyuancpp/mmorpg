package svc

import (
	"context"
	"fmt"
	"time"
)

type routeBootstrapStore interface {
	HasAnyRoute(context.Context) (bool, error)
	HasHistoricalPlayers(context.Context, []uint32) (bool, error)
}
type routeBootstrapCache interface {
	ScanCtx(context.Context, uint64, string, int64) ([]string, uint64, error)
}

// 仅首次空SQL需要穷尽检查；预算耗尽不是空缓存证明。非空SQL仍由原对账水位/逐行maprev守卫处理。
// 导入必须在旧写者停止的窗口执行；本探针只拒漏导入，绝不自动把Redis当当前SQL或替工具解冻。
func checkEmptyRouteBootstrap(ctx context.Context, st routeBootstrapStore, cache routeBootstrapCache, storageIDs []uint32) error {
	bounded, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	hasRows, err := st.HasAnyRoute(bounded)
	if err != nil {
		return fmt.Errorf("读取正规路由SQL失败")
	}
	if hasRows {
		return nil
	}
	hasHistory, err := st.HasHistoricalPlayers(bounded, storageIDs)
	if err != nil {
		return fmt.Errorf("空路由SQL无法证明目录/已配置玩家库为空；先核正规schema与route导入/audit")
	}
	if hasHistory {
		return fmt.Errorf("空路由SQL仍有权威角色/玩家历史；先正规route导入/audit，禁止按新空环境Arm")
	}
	for _, pattern := range []string{"player:zone:*", "player:placement:*", "player:maprev:*", "merge:merged_into:*"} {
		var cursor uint64
		for {
			if err := bounded.Err(); err != nil {
				return fmt.Errorf("空SQL路由启动探针未完成：先执行正规route-tool audit/import，不能猜缓存为空")
			}
			keys, next, err := cache.ScanCtx(bounded, cursor, pattern, 256)
			if err != nil {
				return fmt.Errorf("空SQL路由启动探针失败；未证明历史映射已导入")
			}
			if len(keys) > 0 {
				return fmt.Errorf("SQL路由为空但存在历史映射缓存；停止旧写者后先执行route-tool import-from-redis -allow-db-first -dry-run=false与audit，禁止直接Arm")
			}
			if next == 0 {
				break
			}
			cursor = next
		}
	}
	return nil
}
