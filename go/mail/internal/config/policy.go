// Package config只保留跨清理/观测共享的策略枚举，不维护第二套服务或鉴权配置。
package config

const (
	SweepModeReportOnly = "report_only"
	SweepModeDelete     = "delete"
)
