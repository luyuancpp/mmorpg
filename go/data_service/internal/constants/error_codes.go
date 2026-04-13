package constants

const (
	ErrCodeOK              uint32 = 0
	ErrCodeRedis           uint32 = 1 // Redis operation failed
	ErrCodeLockConflict    uint32 = 2 // Another writer holds the player lock
	ErrCodeVersionMismatch uint32 = 3 // Optimistic lock version mismatch
	ErrCodeNotFound        uint32 = 4 // Player data or mapping not found

	// Snapshot / Rollback errors
	ErrCodeSnapshotNotFound    uint32 = 10 // No snapshot found matching criteria
	ErrCodeSnapshotDBError     uint32 = 11 // MySQL error during snapshot operation
	ErrCodeRollbackFailed      uint32 = 12 // Rollback execution failed
	ErrCodePlayerOnline        uint32 = 13 // Player must be offline for rollback
	ErrCodeInvalidRequest      uint32 = 14 // Missing required fields
	ErrCodeZoneNotFound        uint32 = 15 // Zone has no players
)
