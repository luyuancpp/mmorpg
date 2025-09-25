package reflectutils

import (
	"reflect"
)

// GetStructName 获取传入值的结构体名称
func GetStructName(i interface{}) string {
	t := reflect.TypeOf(i)

	// 处理数组或切片类型
	if t.Kind() == reflect.Slice || t.Kind() == reflect.Array {
		t = t.Elem()
	}

	// 处理指针类型
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}

	// 检查类型是否为结构体
	if t.Kind() == reflect.Struct {
		return t.Name()
	}
	return ""
}
