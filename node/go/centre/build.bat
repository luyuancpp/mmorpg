protoc  ../../../proto/common_proto/tip.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/session.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/empty.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/c2gate.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/account_database.proto --go_out=./pb/game  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/comp.proto --go_out=./pb/game  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
goctl rpc protoc ../../../proto/common_proto/centre_service.proto --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/ --go_out=./ --go-grpc_out=./ --zrpc_out=. -m

pause