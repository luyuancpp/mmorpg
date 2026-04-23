package constants

// Scene types matching the SceneType enum in scene_manager_service.proto.
const (
	SceneTypeMainWorld uint32 = 1
	SceneTypeInstance  uint32 = 2
)

// Scene node types — mirror of proto/common/base/node.proto eSceneNodeType.
// A scene node declares its role at startup (GameConfig.scene_node_type); the
// value is published into etcd via NodeInfo so SceneManager can route world
// vs instance creation to the right pool of nodes. Matching the proto values
// exactly keeps Go and C++ using the same integers on the wire.
const (
	SceneNodeTypeMainWorld      uint32 = 0 // kMainSceneNode       — hosts persistent world channels
	SceneNodeTypeInstance       uint32 = 1 // kSceneNode           — hosts on-demand instances / dungeons
	SceneNodeTypeMainWorldCross uint32 = 2 // kMainSceneCrossNode  — cross-server main world
	SceneNodeTypeInstanceCross  uint32 = 3 // kSceneSceneCrossNode — cross-server instance
)

// NodePurpose classifies a CreateScene call so node selection can filter to
// the appropriate node pool. Mirror co-location intentionally bypasses this
// filter (see createscenelogic.pickInstanceNode) because the gain from
// reusing the source scene's resident map/AI data beats type purity.
type NodePurpose int

const (
	NodePurposeWorld    NodePurpose = 1
	NodePurposeInstance NodePurpose = 2
)

// IsWorldHostingType reports whether a scene node with the given
// scene_node_type can host main-world channels.
func IsWorldHostingType(t uint32) bool {
	return t == SceneNodeTypeMainWorld || t == SceneNodeTypeMainWorldCross
}

// IsInstanceHostingType reports whether a scene node with the given
// scene_node_type can host on-demand instances / dungeons / mirrors.
func IsInstanceHostingType(t uint32) bool {
	return t == SceneNodeTypeInstance || t == SceneNodeTypeInstanceCross
}

// MatchesPurpose returns true when a node with scene_node_type t can serve
// the requested NodePurpose.
func MatchesPurpose(t uint32, purpose NodePurpose) bool {
	switch purpose {
	case NodePurposeWorld:
		return IsWorldHostingType(t)
	case NodePurposeInstance:
		return IsInstanceHostingType(t)
	}
	return false
}
