for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc.exe   --cpp_out=. %%a)
for /f "delims=\" %%a in ('dir /b /a-d /o-d "*.proto"') do (
	protoc.exe --go_out=../../../../node/login/ %%a)
pause