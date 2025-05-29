goctl rpc protoc ../../proto/login/login_service.proto --proto_path=../.. --proto_path=../../third_party/grpc/third_party/protobuf/src/ --go_out=./ --go-grpc_out=./ --zrpc_out=. -m
pause