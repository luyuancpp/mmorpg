package options

import (
	"fmt"
	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/internal/prototools"
)

func init() {

	prototools.RegisterOptionCallback(prototools.OptionTypeMessage,
		func(desc interface{}, opts interface{}) error {

			msg := desc.(protoreflect.MessageDescriptor)

			value := proto.GetExtension(
				opts.(*descriptorpb.MessageOptions),
				messageoption.E_OptionTableName,
			)

			if value != nil {
				fmt.Println("[GO] Message:", msg.Name(), "TableName:", value)
			}

			return nil
		})
}
