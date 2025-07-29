package task

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/pb/taskpb"
	"fmt"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
	"time"
)

func NewDBTaskHandler(redisClient redis.Cmdable) asynq.HandlerFunc {
	return func(ctx context.Context, t *asynq.Task) error {
		var task taskpb.DBTask
		if err := proto.Unmarshal(t.Payload(), &task); err != nil {
			return fmt.Errorf("unmarshal DBTask failed: %v", err)
		}

		mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
		if err != nil {
			return fmt.Errorf("unknown proto message type: %s", task.MsgType)
		}

		msg := dynamicpb.NewMessage(mt.Descriptor())
		if err := proto.Unmarshal(task.Body, msg); err != nil {
			return fmt.Errorf("unmarshal proto message failed: %v", err)
		}

		var (
			resultData []byte
			resultErr  string
		)

		switch task.Op {
		case "read":
			err := db.DB.PBDB.LoadOneByWhereCase(msg, task.WhereCase)
			if err != nil {
				resultErr = fmt.Sprintf("DB read failed: %v", err)
			} else {
				resultData, err = proto.Marshal(msg)
				if err != nil {
					resultErr = fmt.Sprintf("marshal result failed: %v", err)
				}
			}

		case "write":
			err := db.DB.PBDB.Save(msg)
			if err != nil {
				resultErr = fmt.Sprintf("DB write failed: %v", err)
			}

		default:
			resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
		}

		// 写回 TaskResult（只读操作才有结果数据）
		if task.TaskId != "" {
			result := &taskpb.TaskResult{
				Success: resultErr == "",
				Data:    resultData,
				Error:   resultErr,
			}
			resBytes, err := proto.Marshal(result)
			if err != nil {
				return fmt.Errorf("marshal TaskResult failed: %v", err)
			}
			err = redisClient.Set(ctx, task.TaskId, resBytes, time.Minute).Err()
			if err != nil {
				return fmt.Errorf("Redis set TaskResult failed: %v", err)
			}
		}

		// handler 返回 nil 代表任务不会重试
		return nil
	}
}
