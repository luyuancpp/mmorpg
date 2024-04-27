protoc  .\pb\db_base.proto --go_out=./pb/go --go-grpc_out=./pb/go
protoc  .\pb\common.proto --go_out=./pb/go --go-grpc_out=./pb/go
protoc  .\pb\deploy_service.proto --go_out=./pb/go --go-grpc_out=./pb/go

pause