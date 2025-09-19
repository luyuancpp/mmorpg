goctl rpc protoc ../generated/generated_proto/db/proto/service/go/grpc/db/db.proto --proto_path=../generated/generated_proto/db/ --go_out=./db --go-grpc_out=./db --zrpc_out=./db -m
goctl rpc protoc ../generated/generated_proto/login/proto/service/go/grpc/login/db.proto --proto_path=../generated/generated_proto/login/ --go_out=./login --go-grpc_out=./login --zrpc_out=./login -m
pause