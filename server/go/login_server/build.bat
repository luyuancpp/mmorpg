protoc  ../../../proto/common_proto/c2gate.proto --go_out=./  --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/
goctl rpc protoc ../../../proto/common_proto/login_service.proto --proto_path=../../../proto/ --proto_path=../../../third_party/protobuf/src/ --go_out=./ --go-grpc_out=./ --zrpc_out=. -m

pause