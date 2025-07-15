goctl rpc protoc ../proto/instance/instance.proto --proto_path=.. --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./instance --go-grpc_out=./instance --zrpc_out=./instance -m
pause