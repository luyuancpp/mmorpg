package utils

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/pb/taskpb"
	"fmt"
	"github.com/golang/protobuf/proto"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
	"time"
)

func NewDBTaskHandler(redisClient redis.Cmdable) asynq.HandlerFunc {
	return func(ctx context.Context, t *asynq.Task) error {
		var task taskpb.DBTask
		if err := proto.Unmarshal(t.Payload(), &task); err != nil {
			return fmt.Errorf("failed to unmarshal DBTask: %w", err)
		}

		mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
		if err != nil {
			return fmt.Errorf("unknown proto message type: %s", task.MsgType)
		}

		msg := dynamicpb.NewMessage(mt.Descriptor())
		if err := proto.Unmarshal(task.Body, msg); err != nil {
			return fmt.Errorf("failed to unmarshal message body: %w", err)
		}

		switch task.Op {
		case "read":
			db.DB.PBDB.LoadOneByWhereCase(msg, task.WhereCase)
			// 写入结果
			if task.TaskId != "" {
				resultBytes, err := proto.Marshal(msg)
				if err != nil {
					return fmt.Errorf("marshal result failed: %v", err)
				}
				if err := redisClient.Set(ctx, task.TaskId, resultBytes, time.Minute).Err(); err != nil {
					return fmt.Errorf("redis set result failed: %v", err)
				}
			}

		case "write":
			db.DB.PBDB.Save(msg)
		default:
			return fmt.Errorf("unsupported op: %s", task.Op)
		}

		return nil
	}
}
