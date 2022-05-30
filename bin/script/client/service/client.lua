function LoginRequest:ReadyGo() 
	self.account = ("luhailong"..PlayerId.player_id)
	PlayerId.player_id = PlayerId.player_id + 1
end

function ReadyGo()
	request = LoginRequest.new()
	request:ReadyGo()
	player:sendraw(request)
end

function Example()
	responese = TestResponse.new()
	player = responese:add_players()
	player.player_id = 1
	for i=0,responese:players_size() - 1 do
		p = responese:players(i)
		print(p.player_id)
	end
	responese:mutable_error().error_no = 1
	print(responese:error().error_no)
end

function CreatePlayer()
	request = CreatePlayerRequest.new()
	player:sendraw(request)
end

function EnterGame(player_id)
	request = EnterGameRequest.new()
	request.player_id = player_id
	player:sendraw(request)
end

function LeaveGame()
	request = LeaveGameRequest.new()
	player:sendraw(request)
end
