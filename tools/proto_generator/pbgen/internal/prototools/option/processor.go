package prototools

import (
	"google.golang.org/protobuf/types/descriptorpb"
	"sync"
)

func ProcessAllOptions(wg *sync.WaitGroup, fdSet *descriptorpb.FileDescriptorSet) error {
	for _, f := range fdSet.File {
		processFile(f, wg)
	}
	return nil
}

func processFile(f *descriptorpb.FileDescriptorProto, wg *sync.WaitGroup) {
	if f.Options != nil {
		dispatchOption(OptionTypeFile, f, f.Options, wg)
		dispatchExtensions(f, f.Options, wg)
	}

	for _, s := range f.Service {
		if s.Options != nil {
			dispatchOption(OptionTypeService, s, s.Options, wg)
			dispatchExtensions(s, s.Options, wg)
		}
		for _, m := range s.Method {
			if m.Options != nil {
				dispatchOption(OptionTypeMethod, m, m.Options, wg)
				dispatchExtensions(m, m.Options, wg)
			}
		}
	}

	for _, msg := range f.MessageType {
		processMessage(msg, wg)
	}
}

func processMessage(msg *descriptorpb.DescriptorProto, wg *sync.WaitGroup) {
	if msg.Options != nil {
		dispatchOption(OptionTypeMessage, msg, msg.Options, wg)
		dispatchExtensions(msg, msg.Options, wg)
	}

	for _, field := range msg.Field {
		if field.Options != nil {
			dispatchOption(OptionTypeField, field, field.Options, wg)
			dispatchExtensions(field, field.Options, wg)
		}
	}

	for _, nested := range msg.NestedType {
		processMessage(nested, wg)
	}
}
