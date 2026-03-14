package constants

const (
	ErrCodeOK              uint32 = 0
	ErrCodeRedis           uint32 = 1 // Redis operation failed
	ErrCodeLockConflict    uint32 = 2 // Another writer holds the player lock
	ErrCodeVersionMismatch uint32 = 3 // Optimistic lock version mismatch
	ErrCodeNotFound        uint32 = 4 // Player data or mapping not found
)
