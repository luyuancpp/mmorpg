package node

import (
	"context"
	"encoding/json"
	"fmt"
	"go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/namespace"
	"log"
	"login/pb/game"
)

// NodeEventType 表示节点事件类型
type NodeEventType string

const (
	NodeAdded   NodeEventType = "PUT"
	NodeRemoved NodeEventType = "DELETE"
)

// NodeEvent 是节点事件的结构体
type NodeEvent struct {
	Type NodeEventType
	Info *game.NodeInfo
}

// NodeWatcher 是节点监视器
type NodeWatcher struct {
	client *clientv3.Client
	prefix string
}

// NewNodeWatcher 创建一个新的 NodeWatcher 实例
func NewNodeWatcher(client *clientv3.Client, prefix string) *NodeWatcher {
	return &NodeWatcher{client: client, prefix: prefix}
}

// Watch 开启监听，返回一个 channel，通过它实时接收节点事件（上线/下线）
func (nw *NodeWatcher) Watch(ctx context.Context) <-chan NodeEvent {
	events := make(chan NodeEvent)

	// 使用命名空间客户端限定在前缀范围内
	watch := namespace.NewWatcher(nw.client, nw.prefix)

	go func(prefix string) {
		defer close(events)
		rch := watch.Watch(ctx, prefix, clientv3.WithPrefix())

		for wresp := range rch {
			for _, ev := range wresp.Events {
				key := string(ev.Kv.Key)
				var info game.NodeInfo
				if err := json.Unmarshal(ev.Kv.Value, &info); err != nil {
					log.Printf("Invalid NodeInfo JSON for key=%s: %v", key, err)
					continue
				}

				// 打印日志，输出事件的相关信息
				log.Printf("Event Type: %s, Key: %s, NodeInfo: %v", ev.Type, key, info)

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

// Range 获取指定前缀下所有的节点数据
func (nw *NodeWatcher) Range() ([]game.NodeInfo, error) {
	// 使用命名空间客户端限定在前缀范围内
	kv := namespace.NewKV(nw.client, nw.prefix)

	// 获取所有 key
	resp, err := kv.Get(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		return nil, err
	}

	var nodes []game.NodeInfo
	for _, kv := range resp.Kvs {
		var nodeInfo game.NodeInfo
		if err := json.Unmarshal(kv.Value, &nodeInfo); err != nil {
			log.Printf("Invalid NodeInfo JSON for key=%s: %v", string(kv.Key), err)
			continue
		}
		nodes = append(nodes, nodeInfo)
	}

	return nodes, nil
}

// fetchAllNodes 获取所有节点的信息
func (nw *NodeWatcher) FetchAllNodes() ([]game.NodeInfo, error) {
	// 调用 Range 方法获取所有节点信息
	nodes, err := nw.Range()
	if err != nil {
		return nil, fmt.Errorf("failed to fetch nodes: %v", err)
	}
	return nodes, nil
}

// startWatching 启动 Watch，监听节点变动事件
func (nw *NodeWatcher) StartWatching() {
	// 启动 Watch，实时处理节点事件
	events := nw.Watch(context.Background())

	// 处理事件
	for event := range events {
		switch event.Type {
		case NodeAdded:
			// 处理节点添加事件
			log.Printf("Node Added: %+v", event.Info)
		case NodeRemoved:
			// 处理节点删除事件
			log.Printf("Node Removed: %+v", event.Info)
		}
	}
}
