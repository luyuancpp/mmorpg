---<<< BEGIN WRITING YOUR CODE
---<<< END WRITING YOUR CODE

---<<<rpc begin
function EnterSceneC2SProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
---<<< END WRITING YOUR CODE 
end

function PushEnterSceneS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
	--测试连续切换有没有问题
	LogInfo('------------------------>')
	ChangeGsScene(response, own_server_scene1, cross_server_scene1)
	ChangeGsScene(response, cross_server_scene1, own_server_scene1)
	times = times + 1
	LogInfo('<------------------------'..times)
---<<< END WRITING YOUR CODE 
end

function PushSceneInfoS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
	--LogInfo(response:DebugString());
	message = EnterSeceneC2SRequest.new()
	message:mutable_scene_info().scene_id = own_server_scene1
	player:send(message)
---<<< END WRITING YOUR CODE 
end

---<<<rpc end
