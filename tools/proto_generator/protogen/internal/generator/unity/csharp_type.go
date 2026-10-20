package unity

import (
	"fmt"
	"strings"

	"google.golang.org/protobuf/types/descriptorpb"
)

// csharpTypeIndex 把 proto 消息全名(带前导点,如 ".friendpb.AddFriendRequest")映射到 protoc C# 产物里的
// 全限定类型名(如 "global::Friendpb.AddFriendRequest")。
//
// 规则同 protoc 的 C# 生成器:命名空间取文件选项 csharp_namespace,没有则把 package 按 protoc 的
// UnderscoresToCamelCase 转换;嵌套消息位于外层类的 Types 静态类里;map entry 不生成类型。
// 一律带 global:: 前缀:生成的类位于 MmorpgClient.Net.Generated,proto 的 Match / Trade 命名空间与无 package 的
// 全局类型(Empty、TipInfoMessage 等)都可能被客户端里同名的命名空间或类型遮蔽。
//
// 只说明「protoc 会怎样命名」,不保证客户端真的生成了这个类型:客户端只为 tools/gen_proto.ps1 清单里的
// proto 文件出 C#,清单漏一个文件,产物照样引用、客户端编译报 CS0246。
type csharpTypeIndex map[string]string

// newCSharpTypeIndex 由描述符集的全部文件(含依赖)建索引。
func newCSharpTypeIndex(files []*descriptorpb.FileDescriptorProto) csharpTypeIndex {
	index := csharpTypeIndex{}
	for _, file := range files {
		protoPrefix := "."
		if pkg := file.GetPackage(); pkg != "" {
			protoPrefix = "." + pkg + "."
		}
		csharpPrefix := "global::"
		if namespace := csharpNamespace(file); namespace != "" {
			csharpPrefix += namespace + "."
		}
		for _, message := range file.GetMessageType() {
			index.addMessage(protoPrefix+message.GetName(), csharpPrefix+message.GetName(), message)
		}
	}
	return index
}

func (index csharpTypeIndex) addMessage(protoName, csharpName string, message *descriptorpb.DescriptorProto) {
	index[protoName] = csharpName
	for _, nested := range message.GetNestedType() {
		if nested.GetOptions().GetMapEntry() {
			continue
		}
		index.addMessage(protoName+"."+nested.GetName(), csharpName+".Types."+nested.GetName(), nested)
	}
}

// resolve 返回 proto 类型(描述符里的全名,带前导点)对应的 C# 类型名。
func (index csharpTypeIndex) resolve(protoType string) (string, error) {
	if name, ok := index[protoType]; ok {
		return name, nil
	}
	return "", fmt.Errorf("proto type %q is not a message in the descriptor set; cannot name its C# type", protoType)
}

// csharpNamespace 返回 protoc 给这个文件的 C# 命名空间。
func csharpNamespace(file *descriptorpb.FileDescriptorProto) string {
	if options := file.GetOptions(); options != nil && options.CsharpNamespace != nil {
		return options.GetCsharpNamespace()
	}
	return underscoresToPascalCase(file.GetPackage())
}

// underscoresToPascalCase 移植 protoc C# 生成器的 UnderscoresToCamelCase(input, cap_next_letter = true,
// preserve_period = true):小写字母在词首时大写;大写字母原样;数字之后的字母大写;点保留且其后字母大写;
// 其它字符(下划线)丢弃且其后字母大写。例:"friendpb" → "Friendpb","common.base" → "Common.Base",
// "scene_manager" → "SceneManager"。
func underscoresToPascalCase(input string) string {
	var b strings.Builder
	capitalizeNext := true
	for i := 0; i < len(input); i++ {
		c := input[i]
		switch {
		case 'a' <= c && c <= 'z':
			if capitalizeNext {
				c -= 'a' - 'A'
			}
			b.WriteByte(c)
			capitalizeNext = false
		case 'A' <= c && c <= 'Z':
			b.WriteByte(c)
			capitalizeNext = false
		case '0' <= c && c <= '9':
			b.WriteByte(c)
			capitalizeNext = true
		default:
			if c == '.' {
				b.WriteByte('.')
			}
			capitalizeNext = true
		}
	}
	return b.String()
}
