package store

// 第 1b-2 批对账 leader 库原语(route_reconcile.go)给同目录 store_test 包(真库集成用例)的只读出口;_test.go 文件不进生产二进制。
// 与 export_test.go 分开放:那份属于其它批次的用例,本批不改它。

const (
	// RouteScanUpdatedSinceSQLForTest 是按片增量扫描 R29:EXPLAIN 用例对着生产**同一段文本**取计划(抄一份会在改 SQL 时漂移)。
	RouteScanUpdatedSinceSQLForTest = routeScanUpdatedSinceSQL
)
