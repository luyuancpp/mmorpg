package node

import (
	"context"
	"fmt"
	"log"
	"login/internal/config"
	login_proto "login/proto/common/base"
	"sync"

	"github.com/zeromicro/go-zero/core/logx"
	"go.etcd.io/etcd/api/v3/mvccpb"
	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/namespace"
	"google.golang.org/protobuf/encoding/protojson"
)

// NodeEventType represents a node event type.
type NodeEventType string

const (
	NodeAdded   NodeEventType = "PUT"
	NodeRemoved NodeEventType = "DELETE"
)

// NodeEvent represents a node event.
type NodeEvent struct {
	Type NodeEventType
	Info *login_proto.NodeInfo
}

// NodeWatcher watches node events via etcd.
type NodeWatcher struct {
	client *clientv3.Client
	prefix string
}

// NewNodeWatcher creates a new NodeWatcher instance.
func NewNodeWatcher(client *clientv3.Client, prefix string) *NodeWatcher {
	return &NodeWatcher{client: client, prefix: prefix}
}

// Watch starts watching and returns a channel that emits node events (online/offline) in real time.
func (nw *NodeWatcher) Watch(ctx context.Context) <-chan NodeEvent {
	events := make(chan NodeEvent)

	go func(prefix string) {
		defer close(events)

		// Use etcd client Watch directly
		rch := nw.client.Watch(ctx, prefix, clientv3.WithPrefix(), clientv3.WithPrevKV())

		for wresp := range rch {
			for _, ev := range wresp.Events {
				key := string(ev.Kv.Key)
				var info login_proto.NodeInfo
				var data []byte

				// Select data based on event type (Value or PrevKv.Value)
				switch ev.Type {
				case clientv3.EventTypePut:
					data = ev.Kv.Value
				case clientv3.EventTypeDelete:
					data = ev.PrevKv.Value
				}

				// Skip events with no data
				if len(data) == 0 {
					logx.Debugf("Empty data for key=%s, event=%s", key, ev.Type)
					continue
				}

				// Deserialize with protojson
				if err := protojson.Unmarshal(data, &info); err != nil {
					logx.Infof("Invalid protobuf JSON for key=%s: %v", key, err)
					continue
				}

				logx.Infof("Event Type: %s, Key: %s, NodeInfo: %+v", ev.Type, key, info)

				// Send event to channel
				switch ev.Type {
				case clientv3.EventTypePut:
					events <- NodeEvent{Type: NodeAdded, Info: &info}
				case clientv3.EventTypeDelete:
					events <- NodeEvent{Type: NodeRemoved, Info: &info}
				}
			}
		}
	}(nw.prefix)

	return events
}

// Range retrieves all node data under the configured prefix.
func (nw *NodeWatcher) Range() ([]login_proto.NodeInfo, error) {
	kv := namespace.NewKV(nw.client, nw.prefix)

	// Timeout context to avoid blocking indefinitely
	ctx, cancel := context.WithTimeout(context.Background(), config.AppConfig.Timeouts.ServiceDiscoveryTimeout)
	defer cancel()

	resp, err := kv.Get(ctx, "", clientv3.WithPrefix())
	if err != nil {
		logx.Errorf("Failed to get keys with prefix %s: %v", nw.prefix, err)
		return nil, err
	}

	var nodes []login_proto.NodeInfo
	var wg sync.WaitGroup

	// Parse nodes concurrently
	nodeChan := make(chan login_proto.NodeInfo, len(resp.Kvs))

	for _, kv := range resp.Kvs {
		wg.Add(1)
		go func(kv *mvccpb.KeyValue) {
			defer wg.Done()

			var nodeInfo login_proto.NodeInfo
			logx.Debugf("Parsing node data: %s", string(kv.Value))

			if err := protojson.Unmarshal(kv.Value, &nodeInfo); err != nil {
				logx.Errorf("Invalid NodeInfo JSON for key=%s: %v", string(kv.Key), err)
				return
			}

			nodeChan <- nodeInfo
		}(kv)
	}

	// Wait for all goroutines to finish
	go func() {
		wg.Wait()
		close(nodeChan)
	}()

	// Collect parsed nodes
	for nodeInfo := range nodeChan {
		nodes = append(nodes, nodeInfo)
	}

	return nodes, nil
}

// FetchAllNodes retrieves all registered node info.
func (nw *NodeWatcher) FetchAllNodes() ([]login_proto.NodeInfo, error) {
	nodes, err := nw.Range()
	if err != nil {
		return nil, fmt.Errorf("failed to fetch nodes: %v", err)
	}
	return nodes, nil
}

// StartWatching starts the Watch loop and processes node events.
func (nw *NodeWatcher) StartWatching() {
	events := nw.Watch(context.Background())

	for event := range events {
		switch event.Type {
		case NodeAdded:
			log.Printf("Node Added: %+v", event.Info)
		case NodeRemoved:
			log.Printf("Node Removed: %+v", event.Info)
		}
	}
}
