for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc.exe  -I=./ -I=../../../../third_party/protobuf/src --cpp_out=. %%a)
for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc.exe  --proto_path=./ -I=../../../../third_party/protobuf/src --go_out=../../../../node/login/ %%a)
pause