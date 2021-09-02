xlstojson.py
xlstopb.py
for %%a in (*.proto) do (
	protoc  -I=./ -I=../../../../../third_party/protobuf/src --cpp_out=../../../protopb/pbc/ %%a)
pause