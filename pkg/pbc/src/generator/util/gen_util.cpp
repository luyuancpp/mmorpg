inline bool IsTcpNodeType(uint32_t nodeType) {
	static const std::unordered_set<uint32_t> validTypes = {
	
		DeployNodeService,
	
		DbNodeService,
	
		CentreNodeService,
	
		SceneNodeService,
	
		GateNodeService,
	
		LoginNodeService,
	
		RedisNodeService,
	
		EtcdNodeService,
	
		MailNodeService,
	
		ChatNodeService,
	
		TeamNodeService,
	
		ActivityNodeService,
	
		TradeNodeService,
	
		RankNodeService,
	
		TaskNodeService,
	
		GuildNodeService,
	
		MatchNodeService,
	
		AiNodeService,
	
		LogNodeService,
	
		PaymentNodeService,
	
		SecurityNodeService,
	
		CrossServerNodeService,
	
		AnalyticsNodeService,
	
		GmNodeService,
	
	};
	return validTypes.contains(nodeType);
}