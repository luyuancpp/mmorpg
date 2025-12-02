package options

import (
	"fmt"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/prototools"
)

func init() {

	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {

			msg := desc.(protoreflect.MessageDescriptor)

			value := prototools.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				pb.E_OptionTableName,
			)

			if value != nil {
				fmt.Println("[GO] Message:", msg.Name(), "TableName:", value)
			}

			return nil
		})
}
