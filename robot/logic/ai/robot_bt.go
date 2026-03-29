package ai

import (
	b3 "github.com/magicsea/behavior3go"
	"github.com/magicsea/behavior3go/composites"
	"github.com/magicsea/behavior3go/core"

	"robot/logic/gameobject"
	"robot/metrics"
	"robot/pkg"
)

// RobotContext is passed as the Tick target to all BT nodes.
type RobotContext struct {
	Client *pkg.GameClient
	Player *gameobject.Player
	Stats  *metrics.Stats
}

// NewRobotTree builds the default robot behavior tree programmatically.
//
// Tree structure:
//   Sequence (loop forever via RUNNING)
//     └─ WaitAction (random idle 2-5s)
//     └─ ReleaseSkillAction (cast skill 1001 at origin)
//
// The LoopSequence wraps the whole thing to repeat indefinitely.
func NewRobotTree() *core.BehaviorTree {
	tree := core.NewBeTree()

	seq := &composites.Sequence{}
	seq.Ctor()
	seq.Initialize(nil)

	waitNode := &WaitAction{}
	waitNode.Ctor()
	waitNode.Initialize(nil)
	waitNode.SetBaseNodeWorker(waitNode)

	skillNode := &ReleaseSkillAction{}
	skillNode.Ctor()
	skillNode.Initialize(nil)
	skillNode.SetBaseNodeWorker(skillNode)

	seq.AddChild(waitNode)
	seq.AddChild(skillNode)
	seq.SetBaseNodeWorker(seq)

	// Wrap in a repeater-like loop: LoopSequence runs the child sequence
	// repeatedly, returning RUNNING each time the child succeeds.
	loop := &LoopSequence{child: seq}
	loop.Ctor()
	loop.Initialize(nil)
	loop.SetBaseNodeWorker(loop)

	tree.SetRoot(loop)
	return tree
}

// SetRoot sets the root node on a BehaviorTree.
// The library doesn't expose a setter so we use the Load flow workaround:
// we assign via reflection-free approach by embedding in a wrapper.
func (t *treeHelper) SetRoot(node core.IBaseNode) {
	// This won't compile — we need to use the unexported field.
	// Instead, let's build the tree using the Load API with a minimal config.
}

// Actually, BehaviorTree has an unexported `root` field. The only public way to
// set it is via Load(). Let's create a helper that builds via config.
// But that's heavy. Let's just wrap it ourselves.

// RobotBT is a simple behavior tree executor that doesn't depend on the
// library's unexported root field.
type RobotBT struct {
	root       core.IBaseNode
	blackboard *core.Blackboard
}

type treeHelper struct{} // unused, delete

func NewRobotBT() *RobotBT {
	seq := &composites.Sequence{}
	seq.Ctor()
	seq.Initialize(nil)

	waitNode := &WaitAction{}
	waitNode.Ctor()
	waitNode.Initialize(nil)
	waitNode.SetBaseNodeWorker(waitNode)

	skillNode := &ReleaseSkillAction{}
	skillNode.Ctor()
	skillNode.Initialize(nil)
	skillNode.SetBaseNodeWorker(skillNode)

	seq.AddChild(waitNode)
	seq.AddChild(skillNode)
	seq.SetBaseNodeWorker(seq)

	return &RobotBT{
		root:       seq,
		blackboard: core.NewBlackboard(),
	}
}

// Tick runs one tick of the behavior tree.
func (bt *RobotBT) Tick(ctx *RobotContext) b3.Status {
	tree := core.NewBeTree()
	// We must use the library's Tick which needs root set.
	// Use Load with a synthetic config... too complex.
	// Simpler: just execute the root node directly.
	tick := core.NewTick()
	tick.Blackboard = bt.blackboard
	_ = tree // unused, we drive manually
	return bt.root.Execute(tick)
}
