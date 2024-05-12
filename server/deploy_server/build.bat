
goctl rpc protoc deploy_service.proto --go_out=. --go-grpc_out=. --zrpc_out=. -m
protoc  db_base.proto --go_out=. --go-grpc_out=.
pause