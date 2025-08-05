package taskmanager

import "google.golang.org/protobuf/proto"

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
