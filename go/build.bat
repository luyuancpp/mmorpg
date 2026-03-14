goctl rpc protoc ../generated/proto/db/go-zero/service/go/grpc/db/db.proto --proto_path=../generated/proto/db --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./db/proto/ --go-grpc_out=./db/proto/ --zrpc_out=./db -m
rmdir /s /q %cd%\db\service
goctl rpc protoc ../generated/proto/login/go-zero/service/go/grpc/login/login.proto --proto_path=../generated/proto/login  --proto_path=../third_party/grpc/third_party/protobuf/src/ --go_out=./login/proto/ --go-grpc_out=./login/proto/ --zrpc_out=./login -m
rmdir /s /q %cd%\login\service
protoc --go_out=paths=source_relative:./data_service/data_service --go-grpc_out=paths=source_relative:./data_service/data_service --proto_path=../proto/data_service --proto_path=../third_party/grpc/third_party/protobuf/src ../proto/data_service/data_service.proto
pause