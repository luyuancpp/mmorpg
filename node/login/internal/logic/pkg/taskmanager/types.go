package taskmanager

import "google.golang.org/protobuf/proto"

const (
	TaskStatusPending = "pending" // 待处理
	TaskStatusFailed  = "failed"  // 处理失败
	TaskStatusDone    = "done"    // 处理完成
)

type MessageTask struct {
	TaskID     string
	Message    proto.Message
	RedisKey   string
	PlayerID   uint64
	Status     string
	Error      error
	ResultData []byte
}

type TaskBatch struct {
	Tasks []*MessageTask
}
