package svc

import (
	"context"
	"fmt"
	"sync"
	"time"

	"gateway/internal/config"
	commonpb "proto/common/base"

	"github.com/zeromicro/go-zero/core/logx"
	"go.etcd.io/etcd/api/v3/mvccpb"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/encoding/protojson"
)

// ServiceContext holds shared dependencies for the gateway service.
type ServiceContext struct {
	Config      config.Config
	GateWatcher *GateWatcher
}

func NewServiceContext(cfg config.Config) *ServiceContext {
	client, err := clientv3.New(clientv3.Config{
		Endpoints:   cfg.Etcd.Hosts,
		DialTimeout: cfg.Etcd.DialTimeout,
	})
	if err != nil {
		panic(fmt.Errorf("gateway: failed to connect etcd %v: %w", cfg.Etcd.Hosts, err))
	}

	gateNodeType := uint32(commonpb.ENodeType_GateNodeService)
	prefix := fmt.Sprintf("%s.rpc/",
		commonpb.ENodeType_name[int32(gateNodeType)],
	)

	return &ServiceContext{
		Config:      cfg,
		GateWatcher: NewGateWatcher(client, prefix, cfg.Gate.DiscoveryTimeout),
	}
}

// GateWatcher watches etcd for Gate node registrations.
type GateWatcher struct {
	client           *clientv3.Client
	prefix           string
	discoveryTimeout time.Duration
}

func NewGateWatcher(client *clientv3.Client, prefix string, timeout time.Duration) *GateWatcher {
	if timeout <= 0 {
		timeout = 5 * time.Second
	}
	return &GateWatcher{client: client, prefix: prefix, discoveryTimeout: timeout}
}

// FetchAllNodes retrieves all registered gate nodes under the prefix.
func (w *GateWatcher) FetchAllNodes() ([]*commonpb.NodeInfo, error) {
	ctx, cancel := context.WithTimeout(context.Background(), w.discoveryTimeout)
	defer cancel()

	resp, err := w.client.Get(ctx, w.prefix, clientv3.WithPrefix())
	if err != nil {
		logx.Errorf("gateway: failed to fetch gate nodes with prefix %s: %v", w.prefix, err)
		return nil, err
	}

	nodeChan := make(chan *commonpb.NodeInfo, len(resp.Kvs))
	var wg sync.WaitGroup

	for _, item := range resp.Kvs {
		wg.Add(1)
		go func(kv *mvccpb.KeyValue) {
			defer wg.Done()
			info := &commonpb.NodeInfo{}
			if err := protojson.Unmarshal(kv.Value, info); err != nil {
				logx.Errorf("gateway: invalid NodeInfo JSON for key=%s: %v", string(kv.Key), err)
				return
			}
			nodeChan <- info
		}(item)
	}

	go func() {
		wg.Wait()
		close(nodeChan)
	}()

	var nodes []*commonpb.NodeInfo
	for info := range nodeChan {
		nodes = append(nodes, info)
	}
	return nodes, nil
}
