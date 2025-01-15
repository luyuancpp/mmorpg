goctl rpc protoc ../../proto/common/login_service.proto --proto_path=../.. --proto_path=../../third_party/protobuf/src/ --go_out=./ --go-grpc_out=./ --zrpc_out=. -m
goctl rpc protoc ../../proto/common/deploy_service.proto --proto_path=../.. --proto_path=../../third_party/protobuf/src/ --go_out=./ --go-grpc_out=./  --zrpc_out=. -m
pause