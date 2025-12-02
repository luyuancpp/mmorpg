goctl rpc protoc ../generated/generated_proto/db/go-zero/service/go/grpc/db/db.proto --proto_path=../generated/generated_proto/db --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./db/proto/ --go-grpc_out=./db/proto/ --zrpc_out=./db -m
rmdir /s /q %cd%\db\service
goctl rpc protoc ../generated/generated_proto/login/go-zero/service/go/grpc/login/login.proto --proto_path=../generated/generated_proto/login  --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./login/proto/ --go-grpc_out=./login/proto/ --zrpc_out=./login -m
rmdir /s /q %cd%\login\service
pause