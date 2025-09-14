package task

import (
	"context"
	"fmt"
	"game/db/internal/logic/pkg/db"
	"game/db/pb/taskpb"
	"github.com/hibiken/asynq"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/dynamicpb"
)

func NewDBTaskHandler(redisClient redis.Cmdable) asynq.HandlerFunc {
	return func(ctx context.Context, t *asynq.Task) error {
		var task taskpb.DBTask
		if err := proto.Unmarshal(t.Payload(), &task); err != nil {
			logx.Errorf("Failed to unmarshal DBTask payload: %v", err)
			return fmt.Errorf("unmarshal DBTask failed: %v", err)
		}
		logx.Infof("Received DB task: ID=%s, Op=%s, MsgType=%s", task.TaskId, task.Op, task.MsgType)

		mt, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(task.MsgType))
		if err != nil {
			logx.Errorf("Unknown proto message type: %s", task.MsgType)
			return fmt.Errorf("unknown proto message type: %s", task.MsgType)
		}

		msg := dynamicpb.NewMessage(mt.Descriptor())
		if err := proto.Unmarshal(task.Body, msg); err != nil {
			logx.Errorf("Failed to unmarshal proto message body: %v", err)
			return fmt.Errorf("unmarshal proto message failed: %v", err)
		}

		var (
			resultData []byte
			resultErr  string
		)

		switch task.Op {
		case "read":
			logx.Infof("Executing DB read for TaskID=%s", task.TaskId)
			if err := db.DB.PBDB.LoadOneByWhereCase(msg, task.WhereCase); err != nil {
				resultErr = fmt.Sprintf("DB read failed: %v", err)
				logx.Errorf("DB read error for TaskID=%s: %v", task.TaskId, err)
			} else {
				resultData, err = proto.Marshal(msg)
				if err != nil {
					resultErr = fmt.Sprintf("marshal result failed: %v", err)
					logx.Errorf("Marshal result failed for TaskID=%s: %v", task.TaskId, err)
				}
			}

		case "write":
			logx.Infof("Executing DB write for TaskID=%s", task.TaskId)
			if err := db.DB.PBDB.Save(msg); err != nil {
				resultErr = fmt.Sprintf("DB write failed: %v", err)
				logx.Errorf("DB write error for TaskID=%s: %v", task.TaskId, err)
			}

		default:
			resultErr = fmt.Sprintf("unsupported op: %s", task.Op)
			logx.Errorf("Unsupported op for TaskID=%s: %s", task.TaskId, task.Op)
		}

		// 返回结果写入 Redis
		if task.TaskId != "" {
			result := &taskpb.TaskResult{
				Success: resultErr == "",
				Data:    resultData,
				Error:   resultErr,
			}
			resBytes, err := proto.Marshal(result)
			if err != nil {
				logx.Errorf("Marshal TaskResult failed for TaskID=%s: %v", task.TaskId, err)
				return fmt.Errorf("marshal TaskResult failed: %v", err)
			}

			// 用 context.Background() 保证写入 Redis 不被外部取消打断
			err = redisClient.LPush(context.Background(), task.TaskId, resBytes).Err()
			if err != nil {
				logx.Errorf("Failed to write TaskResult to Redis for TaskID=%s: %v", task.TaskId, err)
				return fmt.Errorf("Redis set TaskResult failed: %v", err)
			}

			logx.Infof("TaskResult written to Redis for TaskID=%s", task.TaskId)
		}

		logx.Infof("DB Task %s completed. Success: %v", task.TaskId, resultErr == "")
		return nil
	}
}
