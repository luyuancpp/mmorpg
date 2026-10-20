package internal

import (
	"strings"

	"google.golang.org/protobuf/types/descriptorpb"
)

// SourceCodeInfo.Location.path 里用到的字段号(google/protobuf/descriptor.proto)。
const (
	fileDescriptorServiceField   = 6 // FileDescriptorProto.service
	serviceDescriptorMethodField = 2 // ServiceDescriptorProto.method
)

// MethodLeadingComments 返回方法在 proto 源码里的前导注释,按行切开。
//
// 来源是描述符的 source code info(prototools 生成描述符集时带 --include_source_info)。每行去掉注释符后的
// 一个前导空格与行尾空白,首尾空行丢弃;描述符没有 source info、方法不在所属文件里或没有前导注释时返回 nil。
// 只读描述符,可并发调用。返回的是原文,写进目标语言注释前由调用方按目标语言转义。
func MethodLeadingComments(method *MethodInfo) []string {
	if method == nil || method.Fd == nil || method.ServiceDescriptorProto == nil || method.MethodDescriptorProto == nil {
		return nil
	}
	serviceIndex := indexOfService(method.Fd, method.ServiceDescriptorProto)
	methodIndex := indexOfMethod(method.ServiceDescriptorProto, method.MethodDescriptorProto)
	if serviceIndex < 0 || methodIndex < 0 {
		return nil
	}

	want := []int32{fileDescriptorServiceField, int32(serviceIndex), serviceDescriptorMethodField, int32(methodIndex)}
	for _, location := range method.Fd.GetSourceCodeInfo().GetLocation() {
		if equalLocationPath(location.GetPath(), want) {
			return splitCommentLines(location.GetLeadingComments())
		}
	}
	return nil
}

// indexOfService 按指针找服务在文件里的下标;服务描述符与文件描述符来自同一份描述符集。
func indexOfService(file *descriptorpb.FileDescriptorProto, service *descriptorpb.ServiceDescriptorProto) int {
	for i, candidate := range file.GetService() {
		if candidate == service {
			return i
		}
	}
	return -1
}

func indexOfMethod(service *descriptorpb.ServiceDescriptorProto, method *descriptorpb.MethodDescriptorProto) int {
	for i, candidate := range service.GetMethod() {
		if candidate == method {
			return i
		}
	}
	return -1
}

func equalLocationPath(path, want []int32) bool {
	if len(path) != len(want) {
		return false
	}
	for i := range path {
		if path[i] != want[i] {
			return false
		}
	}
	return true
}

// splitCommentLines 把 protoc 的 leading_comments(形如 " 第一行\n 第二行\n")切成行。
func splitCommentLines(raw string) []string {
	raw = strings.ReplaceAll(raw, "\r", "")
	var lines []string
	for _, line := range strings.Split(raw, "\n") {
		line = strings.TrimRight(line, " \t")
		lines = append(lines, strings.TrimPrefix(line, " "))
	}
	for len(lines) > 0 && lines[0] == "" {
		lines = lines[1:]
	}
	for len(lines) > 0 && lines[len(lines)-1] == "" {
		lines = lines[:len(lines)-1]
	}
	if len(lines) == 0 {
		return nil
	}
	return lines
}
