goctl rpc protoc ../proto/instance/instance.proto --proto_path=../ --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./instance --go-grpc_out=./instance --zrpc_out=./instance -m
goctl rpc protoc ../proto/db/db_service.proto --proto_path=../ --proto_path=../third_party/protobuf/src/ --go_out=./db --go-grpc_out=./db --zrpc_out=./db -m
goctl rpc protoc ../proto/login/login_service.proto --proto_path=../ --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./login --go-grpc_out=./login --zrpc_out=./login -m

pause