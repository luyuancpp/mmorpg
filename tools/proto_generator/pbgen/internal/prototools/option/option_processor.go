package prototools

import (
	"fmt"
	"google.golang.org/protobuf/types/descriptorpb"
	"sync"
)

func ProcessAllOptions(wg *sync.WaitGroup, fdSet *descriptorpb.FileDescriptorSet) error {
	errCh := make(chan error, 64)

	for _, f := range fdSet.File {
		// 每个 f 是 *descriptorpb.FileDescriptorProto
		// 递归处理 message, service 等
		if err := processFileDescriptorProto(f, wg, errCh); err != nil {
			errCh <- err
		}
	}

	wg.Wait()
	close(errCh)

	var allErr error
	for e := range errCh {
		allErr = fmt.Errorf("%w | %v", allErr, e)
	}
	return allErr
}

// 处理单个 FileDescriptorProto
func processFileDescriptorProto(f *descriptorpb.FileDescriptorProto, wg *sync.WaitGroup, errCh chan error) error {
	// FileOptions
	if f.Options != nil {
		dispatchOption(OptionTypeFile, f, f.Options, wg, errCh)
	}

	// ServiceOptions + MethodOptions
	for _, s := range f.Service {
		if s.Options != nil {
			dispatchOption(OptionTypeService, s, s.Options, wg, errCh)
		}
		for _, m := range s.Method {
			if m.Options != nil {
				dispatchOption(OptionTypeMethod, m, m.Options, wg, errCh)
			}
		}
	}

	// MessageOptions + FieldOptions + Nested Messages
	for _, msg := range f.MessageType {
		processMessageProtoRecursive(msg, wg, errCh)
	}

	return nil
}

func processMessageProtoRecursive(msg *descriptorpb.DescriptorProto, wg *sync.WaitGroup, errCh chan error) {
	if msg.Options != nil {
		dispatchOption(OptionTypeMessage, msg, msg.Options, wg, errCh)
	}

	for _, field := range msg.Field {
		if field.Options != nil {
			dispatchOption(OptionTypeField, field, field.Options, wg, errCh)
		}
	}

	for _, nested := range msg.NestedType {
		processMessageProtoRecursive(nested, wg, errCh)
	}
}
