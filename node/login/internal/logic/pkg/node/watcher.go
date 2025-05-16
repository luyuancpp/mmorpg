package node

import (
	"context"
	"encoding/json"
	"log"
	"login/pb/game"
	"strings"

	clientv3 "go.etcd.io/etcd/client/v3"
	clientv3namespace "go.etcd.io/etcd/client/v3/namespace"
)

type NodeEventType string

const (
	NodeAdded   NodeEventType = "PUT"
	NodeRemoved NodeEventType = "DELETE"
)

type NodeEvent struct {
	Type NodeEventType
	Info *game.NodeInfo
}

type NodeWatcher struct {
	client *clientv3.Client
	prefix string
}

// NewNodeWatcher 初始化 watcher
func NewNodeWatcher(client *clientv3.Client, prefix string) *NodeWatcher {
	return &NodeWatcher{client: client, prefix: prefix}
}

// Watch 开启监听，返回一个 channel，通过它实时接收节点事件（上线/下线）
func (nw *NodeWatcher) Watch(ctx context.Context) <-chan NodeEvent {
	events := make(chan NodeEvent)

	// 使用命名空间客户端限定在前缀范围内
	//kv := clientv3namespace.NewKV(nw.client, nw.prefix)
	watch := clientv3namespace.NewWatcher(nw.client, nw.prefix)

	go func() {
		defer close(events)
		rch := watch.Watch(ctx, "", clientv3.WithPrefix())
		for wresp := range rch {
			for _, ev := range wresp.Events {
				key := string(ev.Kv.Key)
				var info game.NodeInfo
				if err := json.Unmarshal(ev.Kv.Value, &info); err != nil {
					log.Printf("Invalid NodeInfo JSON for key=%s: %v", key, err)
					continue
				}
				switch ev.Type {
				case clientv3.EventTypePut:
					events <- NodeEvent{Type: NodeAdded, Info: &info}
				case clientv3.EventTypeDelete:
					// key 中包含 ID，可以从中提取
					//id := extractNodeID(key)
					//info.LeaseId = (id)
					events <- NodeEvent{Type: NodeRemoved, Info: &info}
				}
			}
		}
	}()

	return events
}

// 从 /node/abc 提取 abc
func extractNodeID(key string) string {
	parts := strings.Split(key, "/")
	return parts[len(parts)-1]
}
