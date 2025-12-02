package prototools

import (
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"sync"
)

func ProcessAllOptions(wg *sync.WaitGroup, fdSet *descriptorpb.FileDescriptorSet) {
	for _, f := range fdSet.File {
		// 每个 f 是 *descriptorpb.FileDescriptorProto
		// 递归处理 message, service 等
		if err := processFileDescriptorProto(f, wg); err != nil {
			log.Fatal(err)
		}
	}
}

// 处理单个 FileDescriptorProto
func processFileDescriptorProto(f *descriptorpb.FileDescriptorProto, wg *sync.WaitGroup) error {
	// FileOptions
	if f.Options != nil {
		dispatchOption(OptionTypeFile, f, f.Options, wg)
	}

	// ServiceOptions + MethodOptions
	for _, s := range f.Service {
		if s.Options != nil {
			dispatchOption(OptionTypeService, s, s.Options, wg)
		}
		for _, m := range s.Method {
			if m.Options != nil {
				dispatchOption(OptionTypeMethod, m, m.Options, wg)
			}
		}
	}

	// MessageOptions + FieldOptions + Nested Messages
	for _, msg := range f.MessageType {
		processMessageProtoRecursive(msg, wg)
	}

	return nil
}

func processMessageProtoRecursive(msg *descriptorpb.DescriptorProto, wg *sync.WaitGroup) {
	if msg.Options != nil {
		dispatchOption(OptionTypeMessage, msg, msg.Options, wg)
	}

	for _, field := range msg.Field {
		if field.Options != nil {
			dispatchOption(OptionTypeField, field, field.Options, wg)
		}
	}

	for _, nested := range msg.NestedType {
		processMessageProtoRecursive(nested, wg)
	}
}
