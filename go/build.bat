cd db
goctl rpc protoc ../../generated/proto/db/proto/db/db.proto --proto_path=../../generated/proto/db --proto_path=../../third_party/grpc/third_party/protobuf/src/ --go_out=./proto/ --go-grpc_out=./proto/ --zrpc_out=./ -m
cd ..

cd login
goctl rpc protoc ../../generated/proto/login/proto/login/login.proto --proto_path=../../generated/proto/login --proto_path=../../third_party/grpc/third_party/protobuf/src/ --go_out=./proto/ --go-grpc_out=./proto/ --zrpc_out=./ -m
cd ..

cd scene_manager
protoc ../../generated/proto/scene_manager/proto/scene_manager/scene_manager_service.proto --proto_path=../../generated/proto/scene_manager/proto --proto_path=../../third_party/grpc/third_party/protobuf/src/ --go_out=../ --go-grpc_out=../ --go_opt=module=scene_manager --go-grpc_opt=module=scene_manager
cd ..

protoc --go_out=paths=source_relative:./data_service/data_service --go-grpc_out=paths=source_relative:./data_service/data_service --proto_path=../proto/data_service --proto_path=../third_party/grpc/third_party/protobuf/src ../proto/data_service/data_service.proto
pause