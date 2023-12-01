for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc  -I=./ -I=../../../third_party/protobuf/src --cpp_out=. %%a)
pause