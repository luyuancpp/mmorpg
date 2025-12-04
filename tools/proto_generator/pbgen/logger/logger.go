// pbgen/logger/logger.go
package logger

import (
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
)

// Global 全局logger实例，整个项目共用
var Global *zap.Logger

// Init 初始化全局logger
func Init() error {
	config := zap.NewProductionConfig()
	config.Level = zap.NewAtomicLevelAt(zapcore.InfoLevel)
	config.EncoderConfig.EncodeTime = zapcore.ISO8601TimeEncoder

	var err error
	Global, err = config.Build()
	if err != nil {
		return err
	}

	// 替换zap默认全局logger，方便直接使用zap.L()
	zap.ReplaceGlobals(Global)
	return nil
}

// Sync 同步日志到磁盘（在程序退出时调用）
func Sync() {
	_ = Global.Sync()
}
