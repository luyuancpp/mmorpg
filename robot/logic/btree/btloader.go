package btree

import (
	b3 "github.com/magicsea/behavior3go"
	"github.com/magicsea/behavior3go/config"
	. "github.com/magicsea/behavior3go/core"
	. "github.com/magicsea/behavior3go/loader"
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
)

// RegisterLoginNodes registers custom behavior tree nodes.
func RegisterGameNodes(maps *b3.RegisterStructMaps) {
	maps.Register("CreatePlayer", new(behaviortree.CreatePlayer))
	maps.Register("IsRoleListEmpty", new(behaviortree.IsRoleListEmpty))
	maps.Register("PlayerEnterGame", new(behaviortree.PlayerEnterGame))
	maps.Register("AlreadyLoggedIn", new(behaviortree.AlreadyLoggedIn))
	maps.Register("CheckTestCount", new(behaviortree.CheckTestCount))
	maps.Register("IncrementTestCount", new(behaviortree.IncrementTestCount))
	maps.Register("ResetTestCount", new(behaviortree.ResetTestCount))
	maps.Register("SetSubTree", new(behaviortree.SetSubTree))
	maps.Register("InitTree", new(behaviortree.InitTree))
	maps.Register("BoardEqualConst", new(behaviortree.BoardEqualConst))
	maps.Register("PlayerEnterGame", new(behaviortree.PlayerEnterGame))
	maps.Register("AlreadyLoggedIn", new(behaviortree.AlreadyLoggedIn))
	maps.Register("RandomEnterScene", new(behaviortree.RandomEnterScene))
	maps.Register("CheckTestCount", new(behaviortree.CheckTestCount))
	maps.Register("IncrementTestCount", new(behaviortree.IncrementTestCount))
	maps.Register("ResetTestCount", new(behaviortree.ResetTestCount))
	maps.Register("SetSubTree", new(behaviortree.SetSubTree))
	maps.Register("InitTree", new(behaviortree.InitTree))
	maps.Register("BoardEqualConst", new(behaviortree.BoardEqualConst))
	maps.Register("GetHatredTarget", new(behaviortree.GetHatredTarget))
	maps.Register("SetBoardTargetPos", new(behaviortree.SetBoardTargetPos))
	maps.Register("MoveToEntity", new(behaviortree.MoveToEntity))
	maps.Register("GetTargetDistance", new(behaviortree.GetTargetDistance))
	maps.Register("GetReleasableSkillList", new(behaviortree.GetReleasableSkillList))
	maps.Register("CheckSkillByDistance", new(behaviortree.CheckSkillByDistance))
	maps.Register("GetSkillID", new(behaviortree.GetSkillID))
	maps.Register("ReleaseSkill", new(behaviortree.ReleaseSkill))
	maps.Register("SleepNode", new(behaviortree.SleepNode))
}

func InitPlayerBehaviorTree(configPath string) (map[string]*BehaviorTree, *BehaviorTree, bool) {
	projectConfig, ok := config.LoadRawProjectCfg(configPath)
	if !ok {
		zap.L().Error("Failed to load behavior tree config", zap.String("path", configPath))
		return nil, nil, false
	}

	maps := b3.NewRegisterStructMaps()
	RegisterGameNodes(maps)

	behaviorTree, currentTree := initializePlayerBehaviorTrees(projectConfig, maps)
	if currentTree == nil {
		zap.L().Error("No default behavior tree found")
		return nil, nil, false
	}

	return behaviorTree, currentTree, true
}

// initializeBehaviorTrees initializes behavior trees from configuration.
func initializePlayerBehaviorTrees(config *config.RawProjectCfg, maps *b3.RegisterStructMaps) (map[string]*BehaviorTree, *BehaviorTree) {
	behaviorTree := make(map[string]*BehaviorTree)
	var currentTree *BehaviorTree

	for i, v := range config.Data.Trees {
		tree := CreateBevTreeFromConfig(&v, maps)
		if tree == nil {
			zap.L().Error("Failed to create behavior tree", zap.Int("index", i))
			continue
		}
		if tree.GetTitile() == "Scene" {
			currentTree = tree
		}
		behaviorTree[tree.GetTitile()] = tree
	}

	return behaviorTree, currentTree
}

// InitializeBehaviorTreeBlackboard initializes or resets the behavior tree's blackboard.
func InitializePlayerBehaviorTreeBlackboard(blackboard *Blackboard) {
	blackboard = NewBlackboard()
	blackboard.SetMem(behaviortree.ActorListBoardKey, gameobject.NewActorList())
	blackboard.SetMem(behaviortree.SkillListBoardKey, &game.PlayerSkillListPBComponent{})
	zap.L().Info("Behavior tree blackboard initialized")
}
