package handler

import (
	"go.uber.org/zap"

	"proto/chat"
	"proto/common/base"
	"proto/login"
	"proto/scene"
	"robot/pkg"
)

// --- Chat ---

func handleSendChat(gc *pkg.GameClient, resp *chat.SendChatResponse) {}

func handlePullChatHistory(gc *pkg.GameClient, resp *chat.PullChatHistoryResponse) {}

// --- Login ---

func handleLogin(gc *pkg.GameClient, resp *login.LoginResponse) {
	zap.L().Debug("login response (dispatch)",
		zap.Uint64("player", gc.PlayerId),
		zap.Int("players", len(resp.GetPlayers())),
	)
}

func handleCreatePlayer(gc *pkg.GameClient, resp *login.CreatePlayerResponse) {}

func handleEnterGame(gc *pkg.GameClient, resp *login.EnterGameResponse) {
	zap.L().Debug("enter game response (dispatch)", zap.Uint64("player", gc.PlayerId))
}

func handleLeaveGame(gc *pkg.GameClient, resp *login.LeaveGameRequest) {}

func handleDisconnect(gc *pkg.GameClient, resp *login.LoginNodeDisconnectRequest) {}

// --- Scene common ---

func handleSendTip(gc *pkg.GameClient, resp *base.TipInfoMessage) {}

func handleKickPlayer(gc *pkg.GameClient, resp *scene.GameKickPlayerRequest) {
	zap.L().Warn("kicked by server",
		zap.Uint64("player", gc.PlayerId),
		zap.String("reason", resp.GetReason().String()),
		zap.String("operator", resp.GetOperator()),
	)
}

func handleEnterScene(gc *pkg.GameClient, resp *scene.EnterSceneC2SResponse) {
	zap.L().Info("entered scene", zap.Uint64("player", gc.PlayerId))
}

func handleNotifyEnterScene(gc *pkg.GameClient, resp *scene.EnterSceneS2C) {}

func handleSceneInfoC2S(gc *pkg.GameClient, resp *scene.SceneInfoRequest) {}

func handleNotifySceneInfo(gc *pkg.GameClient, resp *scene.SceneInfoS2C) {}

// --- Scene actors ---

func handleNotifyActorCreate(gc *pkg.GameClient, resp *scene.ActorCreateS2C) {}

func handleNotifyActorDestroy(gc *pkg.GameClient, resp *scene.ActorDestroyS2C) {}

func handleNotifyActorListCreate(gc *pkg.GameClient, resp *scene.ActorListCreateS2C) {}

func handleNotifyActorListDestroy(gc *pkg.GameClient, resp *scene.ActorListDestroyS2C) {}

// --- Skill ---

func handleReleaseSkill(gc *pkg.GameClient, resp *scene.ReleaseSkillResponse) {}

func handleNotifySkillUsed(gc *pkg.GameClient, resp *scene.SkillUsedS2C) {}

func handleNotifySkillInterrupted(gc *pkg.GameClient, resp *scene.SkillInterruptedS2C) {}

func handleListSkills(gc *pkg.GameClient, resp *scene.ListSkillsResponse) {}
