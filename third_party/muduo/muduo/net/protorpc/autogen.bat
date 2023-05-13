for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc  -I=./ -I=../../../../protobuf/src --cpp_out=. %%a)
pause