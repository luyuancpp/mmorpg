package node

import (
	"context"
	"fmt"
	"github.com/zeromicro/go-zero/core/logx"
	"go.etcd.io/etcd/api/v3/mvccpb"
	"go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/namespace"
	"google.golang.org/protobuf/encoding/protojson"
	"log"
	"login/internal/config"
	login_proto "login/proto/common"
	"sync"
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
	Info *login_proto.NodeInfo
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

	go func(prefix string) {
		defer close(events)

		// 直接用 etcd client 的 Watch 方法
		rch := nw.client.Watch(ctx, prefix, clientv3.WithPrefix(), clientv3.WithPrevKV())

		for wresp := range rch {
			for _, ev := range wresp.Events {
				key := string(ev.Kv.Key)
				var info login_proto.NodeInfo
				var data []byte

				// 根据事件类型，选择正确的 data（Value 或 PrevKv.Value）
				switch ev.Type {
				case clientv3.EventTypePut:
					data = ev.Kv.Value
				case clientv3.EventTypeDelete:
					data = ev.PrevKv.Value
				}

				// 如果没有数据，跳过该事件
				if len(data) == 0 {
					logx.Debugf("Empty data for key=%s, event=%s", key, ev.Type)
					continue
				}

				// 使用 protojson.Unmarshal 直接反序列化数据
				if err := protojson.Unmarshal(data, &info); err != nil {
					logx.Infof("Invalid protobuf JSON for key=%s: %v", key, err)
					continue
				}

				logx.Infof("Event Type: %s, Key: %s, NodeInfo: %+v", ev.Type, key, info)

				// 根据事件类型将数据发送到 events 通道
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
func (nw *NodeWatcher) Range() ([]login_proto.NodeInfo, error) {
	// 使用命名空间客户端限定在前缀范围内
	kv := namespace.NewKV(nw.client, nw.prefix)

	// 设置带有超时的 context，避免长时间阻塞
	ctx, cancel := context.WithTimeout(context.Background(), config.AppConfig.Timeouts.ServiceDiscoveryTimeout)
	defer cancel()

	// 获取所有 key
	resp, err := kv.Get(ctx, "", clientv3.WithPrefix())
	if err != nil {
		logx.Errorf("Failed to get keys with prefix %s: %v", nw.prefix, err)
		return nil, err
	}

	var nodes []login_proto.NodeInfo
	var wg sync.WaitGroup

	// 使用 channel 和 goroutine 并发解析每个节点
	nodeChan := make(chan login_proto.NodeInfo, len(resp.Kvs))

	for _, kv := range resp.Kvs {
		wg.Add(1)
		go func(kv *mvccpb.KeyValue) {
			defer wg.Done()

			var nodeInfo login_proto.NodeInfo
			logx.Debugf("Parsing node data: %s", string(kv.Value)) // 简单输出，以避免过多日志

			// 解析 JSON 数据
			if err := protojson.Unmarshal(kv.Value, &nodeInfo); err != nil {
				logx.Errorf("Invalid NodeInfo JSON for key=%s: %v", string(kv.Key), err)
				return
			}

			// 发送解析结果到 channel
			nodeChan <- nodeInfo
		}(kv)
	}

	// 等待所有 goroutine 完成解析
	go func() {
		wg.Wait()
		close(nodeChan)
	}()

	// 收集所有解析成功的节点信息
	for nodeInfo := range nodeChan {
		nodes = append(nodes, nodeInfo)
	}

	return nodes, nil
}

// fetchAllNodes 获取所有节点的信息
func (nw *NodeWatcher) FetchAllNodes() ([]login_proto.NodeInfo, error) {
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
