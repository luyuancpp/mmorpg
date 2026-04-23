package constants

// Scene manager business error codes returned in gRPC response fields.
const (
	ErrNoAvailableNode   uint32 = 1
	ErrSceneLookupFailed uint32 = 2
	ErrUpdateLocation    uint32 = 3
	ErrInvalidNodeID     uint32 = 4
	ErrInvalidGateID     uint32 = 5
	ErrEncodeEvent       uint32 = 6
	ErrKafkaRoute        uint32 = 7
	ErrRedis             uint32 = 8
	ErrDuplicateScene    uint32 = 9
	ErrInvalidSceneType  uint32 = 10
	ErrNoSceneConfId     uint32 = 11
	// ErrNoNodeForPurpose: no scene node with a matching scene_node_type is
	// registered in the target zone. Returned when StrictNodeTypeSeparation is
	// enabled and the caller requests a world/instance scene but the zone's
	// node pool has no node able to host that purpose.
	ErrNoNodeForPurpose uint32 = 12
)
