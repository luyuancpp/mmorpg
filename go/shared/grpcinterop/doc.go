// Package grpcinterop 只放 grpc-go 与 C++ muduo_grpc 之间的互通测试
// (docs/design/grpc-muduo-transport.md §15.14 第 6 步),没有可供引用的生产代码。
// 用例默认跳过,设环境变量才连真实的 C++ 进程,见 muduo_server_interop_test.go。
package grpcinterop
