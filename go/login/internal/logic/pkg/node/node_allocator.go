package node

import (
	"context"
	"fmt"
	login_proto "proto/common/base"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"
)

type NodeAllocator struct {
	Client *clientv3.Client
	Prefix string
}

func NewNodeAllocator(client *clientv3.Client, prefix string) *NodeAllocator {
	return &NodeAllocator{Client: client, Prefix: prefix}
}

// buildAllocationKey returns the global-uniqueness CAS key for (nodeType, nodeID).
// It's intentionally zone-independent so that two zones cannot both claim the
// same (nodeType, nodeID) pair: the per-type global lock prevents the Snowflake
// seed collision we observed when zone 3 and zone 4 Login nodes both ended up
// with node_id=1 after a transient z3 lease drop.
func buildAllocationKey(prefix string, nodeType, nodeID uint32) string {
	return fmt.Sprintf("%s/allocated/node_type/%d/node_id/%d", prefix, nodeType, nodeID)
}

// TryAllocateNodeID atomically assigns a globally-unique node_id for the given
// (node_type) across all zones. Reuses free slots (lease expired) by scanning
// the allocation registry for gaps.
//
// Invariant: within a given node_type, every live node has a distinct node_id,
// regardless of zone. This is required because the Snowflake PlayerId seed
// uses node_id directly — if two zones both see node_id=1 they generate
// identical PlayerIds and the reverse `player_id → account` index mis-routes
// EnterGame requests across zones.
func (na *NodeAllocator) TryAllocateNodeID(ctx context.Context, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (uint32, error) {
	// Scan the global allocation registry for this node_type. We use a
	// dedicated sub-prefix so this lookup is O(live nodes of one type),
	// not O(all services + all zones + all node_ids).
	allocPrefix := fmt.Sprintf("%s/allocated/node_type/%d/node_id/", na.Prefix, info.NodeType)
	resp, err := na.Client.Get(ctx, allocPrefix, clientv3.WithPrefix())
	if err != nil {
		return 0, err
	}

	usedIDs := make(map[uint32]bool)
	maxID := uint32(0)
	for _, kv := range resp.Kvs {
		var id uint32
		// Key tail after allocPrefix is the numeric node_id.
		if _, err := fmt.Sscanf(string(kv.Key[len(allocPrefix):]), "%d", &id); err != nil {
			continue
		}
		usedIDs[id] = true
		if id > maxID {
			maxID = id
		}
	}

	searchRange := maxID + 10

	// Search upward for an available ID. CAS on the global allocation key
	// protects against two concurrently-starting nodes racing on the same id.
	for id := uint32(0); id < searchRange; id++ {
		if usedIDs[id] {
			continue
		}
		ok, err := na.putIfAbsent(ctx, id, info, leaseID)
		if err != nil {
			continue
		}
		if ok {
			return id, nil
		}
	}

	return 0, fmt.Errorf("failed to allocate node ID")
}

// putIfAbsent atomically claims (nodeType, nodeID) globally, then publishes the
// full per-zone NodeInfo record under the same lease so both evaporate together
// when the node dies.
//
// Both writes share the same leaseID, so a partial failure (e.g. process killed
// between them) is self-cleaning: the allocation key expires with the lease
// and the slot becomes reusable on next startup scan.
func (na *NodeAllocator) putIfAbsent(ctx context.Context, nodeID uint32, info *login_proto.NodeInfo, leaseID clientv3.LeaseID) (bool, error) {
	allocKey := buildAllocationKey(na.Prefix, info.NodeType, nodeID)
	infoKey := BuildRpcPath(na.Prefix, info.ZoneId, info.NodeType, nodeID)

	info.NodeId = nodeID
	result, err := protojson.Marshal(info)
	if err != nil {
		logx.Errorf("Error marshaling: %v", err)
		return false, err
	}
	resultStr := string(result)

	// The allocation key's value is the node uuid, purely for diagnostics —
	// the key's existence is what guards uniqueness.
	txn := na.Client.Txn(ctx)
	txnResp, err := txn.If(
		clientv3.Compare(clientv3.Version(allocKey), "=", 0),
	).Then(
		clientv3.OpPut(allocKey, info.NodeUuid, clientv3.WithLease(leaseID)),
		clientv3.OpPut(infoKey, resultStr, clientv3.WithLease(leaseID)),
	).Commit()

	if err != nil {
		return false, err
	}
	return txnResp.Succeeded, nil
}
