function LoginRequest:ReadyGo() 
	self.account = ("luhailong"..PlayerId.guid)
	PlayerId.guid = PlayerId.guid + 1
end

function ReadyGo()
	request = LoginRequest.new()
	request:ReadyGo()
	player:send(request)
end

function Example()
	responese = TestResponse.new()
	player = responese:add_players()
	player.guid = 1
	for i=0,responese:players_size() - 1 do
		p = responese:players(i)
		print(p.guid)
	end
	responese:mutable_error().error_no = 1
	print(responese:error().error_no)
end

function CreatePlayer()
	request = CreatePlayerRequest.new()
	player:send(request)
end

function EnterGame(guid)
	request = EnterGameRequest.new()
	request.guid = guid
	player:send(request)
end

function LeaveGame()
	request = LeaveGameRequest.new()
	player:send(request)
end