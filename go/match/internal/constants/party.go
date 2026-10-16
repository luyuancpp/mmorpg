package constants

// 队伍补位匹配(docs/design/team-backfill-matching.md)里 logic 与 team 两个包之间传递的码值。
//
// 两个包互不 import(team 依赖 proto/team 生成物,logic 不能跟着它一起在 regen 前编不过),
// 接口方法签名只用内建类型,所以码值放在这个两边都能 import 的包里。这些码不下发客户端,
// 不占 tip 段,加减不需要导表。

// 补位排队态:TeamBattleStarter.PartyQueueState 的返回值,team 据此装饰队伍视图。
const (
	// PartyQueueNone 该玩家没有本队的补位票(没在排、排的是别的单,或已经开战)。
	PartyQueueNone uint32 = 0
	// PartyQueueQueued 补位票在队列里等凑满。
	PartyQueueQueued uint32 = 1
	// PartyQueueMatched 已凑满弹出,开局管线在途(取消太迟)。
	PartyQueueMatched uint32 = 2
)

// 补位单被撤出队列(不是成功开战)的原因:PartyTeamPort.CheckParty 的判定结果,
// 也是 PartyTeamPort.PartyEnded 的通知参数。0 = 队伍仍然有效(只用于 CheckParty)。
const (
	// PartyEndNone CheckParty:队伍与名单仍然有效。作为撤单原因传给 CancelParty 时表示静默撤单(不通知队员)。
	PartyEndNone uint32 = 0
	// PartyEndCanceled 队员主动取消(culprit = 取消者)。
	PartyEndCanceled uint32 = 1
	// PartyEndRosterChanged 队伍名单变了或队伍已不存在(culprit = 0)。
	PartyEndRosterChanged uint32 = 2
	// PartyEndMemberOffline 有队员不在线(culprit = 该队员)。
	PartyEndMemberOffline uint32 = 3
	// PartyEndMemberNotReady 有队员的票据失效、已在战斗中,或开局时备战失败(culprit = 该队员,可能为 0)。
	PartyEndMemberNotReady uint32 = 4
)
