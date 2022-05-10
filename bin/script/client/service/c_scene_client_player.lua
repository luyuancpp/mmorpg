---<<< BEGIN WRITING YOUR CODE
---<<< END WRITING YOUR CODE

---<<<rpc begin
function EnterSceneC2SProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
---<<< END WRITING YOUR CODE 
end

function PushEnterSceneS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
---<<< END WRITING YOUR CODE 
end

function PushSceneInfoS2CProcess(request, response)
---<<< BEGIN WRITING YOUR CODE 
	message = EnterSeceneC2SRequest.new()
	message:mutable_scene_info().scene_confid = 2
	player:send(message)
---<<< END WRITING YOUR CODE 
end

---<<<rpc end
