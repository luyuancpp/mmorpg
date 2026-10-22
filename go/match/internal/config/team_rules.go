package config

// Team持久状态机硬规则。队伍规则与全球策略摘要共同读取这一事实源，
// 避免不同副本对同一SQL事务域使用不同人数、期限或容量限制。
const (
	TeamCapacity                           = 5
	TeamApplicationTTLMs            uint64 = 120_000
	TeamMaxApplications                    = 10
	TeamInviteTTLMs                 uint64 = 60_000
	TeamMaxInvitesPerTeam                  = 10
	TeamMaxPendingInvitesPerInvitee        = 10
	TeamIdleTTLSeconds                     = 86400
)
