---<<< BEGIN WRITING YOUR CODE
local own_server_scene1 = 4294967298
local own_server_scene2 = 4294967297
local cross_server_scene1 = 1
local cross_server_scene2 = 2
local times = 0

function ChangeGsScene(response, form_scene, to_scene)
	message = EnterSeceneC2SRequest.new()
	message:mutable_scene_info().scene_id = to_scene
	player:send(message)
	LogInfo(response:DebugString())
end
---<<< END WRITING YOUR CODE

---<<<rpc begin
function EnterSceneC2SProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
---<<< END WRITING YOUR CODE 
end

function PushEnterSceneS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
	--连续切换有问题
	LogInfo('------------------------>')
	ChangeGsScene(response, cross_server_scene1, cross_server_scene1)
	times = times + 1
	LogInfo('<------------------------'..times)
---<<< END WRITING YOUR CODE 
end

function PushSceneInfoS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
	LogInfo(response:DebugString());
	message = EnterSeceneC2SRequest.new()
	message:mutable_scene_info().scene_id = cross_server_scene1
	player:send(message)
---<<< END WRITING YOUR CODE 
end

---<<<rpc end
