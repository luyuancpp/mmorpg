// protogen/logger/logger.go
package logger

import (
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
)

// Global is the shared logger instance.
var Global *zap.Logger

// Init initializes the global logger.
func Init() error {
	config := zap.NewProductionConfig()
	config.Level = zap.NewAtomicLevelAt(zapcore.InfoLevel)
	config.EncoderConfig.EncodeTime = zapcore.ISO8601TimeEncoder

	var err error
	Global, err = config.Build()
	if err != nil {
		return err
	}

	zap.ReplaceGlobals(Global)
	return nil
}

// Sync flushes buffered log entries. Call before program exit.
func Sync() {
	_ = Global.Sync()
}
