package prototools

import (
	"fmt"
	"google.golang.org/protobuf/reflect/protoreflect"
	"sync"
)

// ProcessAllOptions 开始处理所有 option
func ProcessAllOptions(wg *sync.WaitGroup, files []protoreflect.FileDescriptor) error {

	errCh := make(chan error, 64)

	for _, f := range files {

		// ----------------------
		// File Options
		// ----------------------
		if opts := f.Options(); opts != nil {
			dispatchOption(OptionTypeFile, f, opts, wg, errCh)
		}

		// ----------------------
		// Service Options
		// ----------------------
		for i := 0; i < f.Services().Len(); i++ {
			s := f.Services().Get(i)
			if opts := s.Options(); opts != nil {
				dispatchOption(OptionTypeService, s, opts, wg, errCh)
			}

			// Method
			for j := 0; j < s.Methods().Len(); j++ {
				m := s.Methods().Get(j)
				if opts := m.Options(); opts != nil {
					dispatchOption(OptionTypeMethod, m, opts, wg, errCh)
				}
			}
		}

		// ----------------------
		// Message Options
		// ----------------------
		for i := 0; i < f.Messages().Len(); i++ {
			msg := f.Messages().Get(i)
			processMessageRecursive(msg, wg, errCh)
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

// 递归处理 message + 子 message
func processMessageRecursive(msg protoreflect.MessageDescriptor, wg *sync.WaitGroup, errCh chan error) {

	if opts := msg.Options(); opts != nil {
		dispatchOption(OptionTypeMessage, msg, opts, wg, errCh)
	}

	// Field Options
	fields := msg.Fields()
	for i := 0; i < fields.Len(); i++ {
		fd := fields.Get(i)
		if opts := fd.Options(); opts != nil {
			dispatchOption(OptionTypeField, fd, opts, wg, errCh)
		}
	}

	// 子 message
	for i := 0; i < msg.Messages().Len(); i++ {
		processMessageRecursive(msg.Messages().Get(i), wg, errCh)
	}
}
