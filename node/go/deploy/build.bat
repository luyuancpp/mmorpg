protoc  ../../../proto/common_proto/tip.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
protoc  ../../../proto/common_proto/common.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
goctl rpc protoc ../../../proto/common_proto/deploy_service.proto --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/ --go_out=./ --go-grpc_out=./ --zrpc_out=. -m
pause