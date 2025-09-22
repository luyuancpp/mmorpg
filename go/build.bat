goctl rpc protoc ../generated/generated_proto/db/go-zero_proto/service/go/grpc/db/db.proto --proto_path=../generated/generated_proto/db --go_out=./db --go-grpc_out=./db --zrpc_out=./db -m
rmdir /s /q %cd%\db\service
goctl rpc protoc ../generated/generated_proto/login/proto/service/go/grpc/login/login.proto --proto_path=../generated/generated_proto/login/ --go_out=./login --go-grpc_out=./login --zrpc_out=./login -m
pause