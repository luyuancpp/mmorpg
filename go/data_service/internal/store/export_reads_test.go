package store

// 第 3 批读方(route_reads.go)给同目录 store_test 包(真库集成用例)的只读出口;_test.go 文件不进生产二进制。
// 与 export_test.go 分开放:那份属于其它 store 的用例,本批不改它。

const (
	// RouteFenceReadShardForTest 是读方读 zone_merge_fence 的分片号(63):真库用例按它造「只在这一片生效」的围栏。
	RouteFenceReadShardForTest = routeFenceReadShard

	// 让 EXPLAIN 用例对着生产**同一段文本**取计划(抄一份就会在改 SQL 时与生产漂移)。
	RouteReadHomeFenceSQLForTest    = routeReadHomeFenceSQL
	RouteScanByHomeSQLForTest       = routeScanByHomeSQL
	RouteScanByStorageSQLForTest    = routeScanByStorageSQL
	RouteListActiveFencesSQLForTest = routeListActiveFencesSQL
)
