package prototools

import (
	"fmt"
	"sync"

	"go.uber.org/zap" // 引入zap结构化日志字段
	"pbgen/logger"    // 引入全局logger包
)

func dispatchOption(t OptionType, desc interface{}, opts interface{}, wg *sync.WaitGroup) {
	callbacks := getCallbacks(t)

	// 空回调快速返回，避免无效并发
	if len(callbacks) == 0 {
		logger.Global.Debug("无可用的Option回调",
			zap.Int("option_type", int(t)),
		)
		return
	}

	logger.Global.Debug("开始分发Option回调",
		zap.Int("option_type", int(t)),
		zap.Int("callback_count", len(callbacks)),
	)

	for _, cb := range callbacks {
		wg.Add(1)
		go func(fn OptionCallback) {
			defer wg.Done()

			if err := fn(desc, opts); err != nil {
				logger.Global.Error("Option回调执行失败",
					zap.Int("option_type", int(t)),
					zap.Error(err),
					zap.String("desc_type", getTypeString(desc)),
					zap.String("opts_type", getTypeString(opts)),
				)
				return
			}

			logger.Global.Debug("Option回调执行成功",
				zap.Int("option_type", int(t)),
				zap.String("desc_type", getTypeString(desc)),
				zap.String("opts_type", getTypeString(opts)),
			)
		}(cb)
	}
}

// getTypeString 获取接口类型的字符串描述（辅助日志上下文）
func getTypeString(v interface{}) string {
	if v == nil {
		return "nil"
	}
	return fmt.Sprintf("%T", v)
}
