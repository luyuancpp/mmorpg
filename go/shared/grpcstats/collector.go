// Package grpcstats provides a gRPC unary server interceptor that tracks
// per-method traffic statistics (request/response byte counts, call counts,
// latency). Statistics are collected only when enabled, and a background
// reporter periodically logs a summary.
//
// Usage:
//
//	collector := grpcstats.New(grpcstats.Options{})
//	server.AddUnaryInterceptors(collector.UnaryServerInterceptor())
//	// Toggle at runtime:
//	collector.Enable(0)            // 0 = no auto-disable
//	collector.Enable(30*time.Minute) // auto-disable after 30 min
//	collector.Disable()
package grpcstats

import (
	"context"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
)

// MethodStats holds atomic counters for a single gRPC method.
type MethodStats struct {
	RecvCount  atomic.Int64
	RecvBytes  atomic.Int64
	RespBytes  atomic.Int64
	LatencySum atomic.Int64 // microseconds
	MaxReqSize atomic.Int64
	MaxLatency atomic.Int64 // microseconds
}

// MethodSnapshot is a point-in-time copy of MethodStats.
type MethodSnapshot struct {
	FullMethod string
	RecvCount  int64
	RecvBytes  int64
	RespBytes  int64
	LatencyAvg time.Duration
	MaxReqSize int64
	MaxLatency time.Duration
}

func (s MethodSnapshot) TotalBytes() int64 { return s.RecvBytes + s.RespBytes }

// Options configures the stats collector.
type Options struct {
	// ReportInterval is how often summaries are logged. Default: 30s.
	// Overridden by env GRPC_TRAFFIC_STATS_INTERVAL_SECONDS.
	ReportInterval time.Duration

	// TopN limits how many methods are listed in each report. Default: 20.
	TopN int
}

// Collector gathers per-method gRPC traffic statistics.
type Collector struct {
	enabled atomic.Bool
	methods sync.Map // fullMethod string → *MethodStats

	reportInterval time.Duration
	topN           int
	autoDisable    time.Time

	mu         sync.Mutex
	stopReport chan struct{}
	reporting  bool
}

// New creates a Collector. If env GRPC_TRAFFIC_STATS_ENABLED=1 is set,
// collection starts immediately.
func New(opts Options) *Collector {
	interval := opts.ReportInterval
	if interval <= 0 {
		interval = 30 * time.Second
	}
	if envVal := os.Getenv("GRPC_TRAFFIC_STATS_INTERVAL_SECONDS"); envVal != "" {
		if v, err := strconv.Atoi(envVal); err == nil && v > 0 && v <= 3600 {
			interval = time.Duration(v) * time.Second
		}
	}

	topN := opts.TopN
	if topN <= 0 {
		topN = 20
	}

	c := &Collector{
		reportInterval: interval,
		topN:           topN,
	}

	if os.Getenv("GRPC_TRAFFIC_STATS_ENABLED") == "1" {
		autoMinutes := 0
		if envVal := os.Getenv("GRPC_TRAFFIC_STATS_AUTO_DISABLE_MINUTES"); envVal != "" {
			if v, err := strconv.Atoi(envVal); err == nil && v > 0 {
				autoMinutes = v
			}
		}
		c.Enable(time.Duration(autoMinutes) * time.Minute)
	}

	return c
}

// Enable starts statistics collection.
// autoDisableAfter > 0 schedules automatic disable after that duration.
func (c *Collector) Enable(autoDisableAfter time.Duration) {
	c.methods.Range(func(key, _ any) bool {
		c.methods.Delete(key)
		return true
	})

	c.mu.Lock()
	defer c.mu.Unlock()

	if autoDisableAfter > 0 {
		c.autoDisable = time.Now().Add(autoDisableAfter)
	} else {
		c.autoDisable = time.Time{}
	}

	c.enabled.Store(true)

	if !c.reporting {
		c.stopReport = make(chan struct{})
		c.reporting = true
		go c.reportLoop(c.stopReport)
	}

	logx.Infof("[grpcstats] Enabled. interval=%v auto_disable=%v", c.reportInterval, autoDisableAfter)
}

// Disable stops statistics collection and logs a final report.
func (c *Collector) Disable() {
	c.enabled.Store(false)

	c.mu.Lock()
	if c.reporting {
		close(c.stopReport)
		c.reporting = false
	}
	c.mu.Unlock()

	c.reportOnce()
	logx.Info("[grpcstats] Disabled.")
}

// IsEnabled returns the current state.
func (c *Collector) IsEnabled() bool {
	return c.enabled.Load()
}

// UnaryServerInterceptor returns a gRPC interceptor that records stats.
func (c *Collector) UnaryServerInterceptor() grpc.UnaryServerInterceptor {
	return func(
		ctx context.Context,
		req interface{},
		info *grpc.UnaryServerInfo,
		handler grpc.UnaryHandler,
	) (interface{}, error) {
		if !c.enabled.Load() {
			return handler(ctx, req)
		}

		reqSize := int64(0)
		if msg, ok := req.(proto.Message); ok {
			reqSize = int64(proto.Size(msg))
		}

		start := time.Now()
		resp, err := handler(ctx, req)
		latency := time.Since(start)

		respSize := int64(0)
		if msg, ok := resp.(proto.Message); ok {
			respSize = int64(proto.Size(msg))
		}

		stats := c.getOrCreateStats(info.FullMethod)
		stats.RecvCount.Add(1)
		stats.RecvBytes.Add(reqSize)
		stats.RespBytes.Add(respSize)
		stats.LatencySum.Add(latency.Microseconds())

		// Update max request size
		for {
			cur := stats.MaxReqSize.Load()
			if reqSize <= cur || stats.MaxReqSize.CompareAndSwap(cur, reqSize) {
				break
			}
		}

		// Update max latency
		latUs := latency.Microseconds()
		for {
			cur := stats.MaxLatency.Load()
			if latUs <= cur || stats.MaxLatency.CompareAndSwap(cur, latUs) {
				break
			}
		}

		return resp, err
	}
}

func (c *Collector) getOrCreateStats(fullMethod string) *MethodStats {
	if v, ok := c.methods.Load(fullMethod); ok {
		return v.(*MethodStats)
	}
	v, _ := c.methods.LoadOrStore(fullMethod, &MethodStats{})
	return v.(*MethodStats)
}

func (c *Collector) reportLoop(stop chan struct{}) {
	ticker := time.NewTicker(c.reportInterval)
	defer ticker.Stop()

	for {
		select {
		case <-stop:
			return
		case <-ticker.C:
			// Check auto-disable
			c.mu.Lock()
			autoDisable := c.autoDisable
			c.mu.Unlock()

			if !autoDisable.IsZero() && time.Now().After(autoDisable) {
				c.enabled.Store(false)
				logx.Info("[grpcstats] Auto-disabled after timeout.")
				c.reportOnce()
				c.mu.Lock()
				c.reporting = false
				c.mu.Unlock()
				return
			}

			c.reportOnce()
		}
	}
}

func (c *Collector) reportOnce() {
	var snapshots []MethodSnapshot

	c.methods.Range(func(key, value any) bool {
		method := key.(string)
		stats := value.(*MethodStats)

		recvCount := stats.RecvCount.Swap(0)
		recvBytes := stats.RecvBytes.Swap(0)
		respBytes := stats.RespBytes.Swap(0)
		latencySum := stats.LatencySum.Swap(0)
		maxReqSize := stats.MaxReqSize.Swap(0)
		maxLatency := stats.MaxLatency.Swap(0)

		if recvCount == 0 {
			return true
		}

		snapshots = append(snapshots, MethodSnapshot{
			FullMethod: method,
			RecvCount:  recvCount,
			RecvBytes:  recvBytes,
			RespBytes:  respBytes,
			LatencyAvg: time.Duration(latencySum/recvCount) * time.Microsecond,
			MaxReqSize: maxReqSize,
			MaxLatency: time.Duration(maxLatency) * time.Microsecond,
		})
		return true
	})

	if len(snapshots) == 0 {
		logx.Infof("[grpcstats] window=%v no traffic", c.reportInterval)
		return
	}

	sort.Slice(snapshots, func(i, j int) bool {
		return snapshots[i].TotalBytes() > snapshots[j].TotalBytes()
	})

	var totalRecv, totalResp int64
	var totalCount int64
	for _, s := range snapshots {
		totalRecv += s.RecvBytes
		totalResp += s.RespBytes
		totalCount += s.RecvCount
	}

	logx.Infof("[grpcstats] window=%v total_calls=%d req_bytes=%s resp_bytes=%s",
		c.reportInterval, totalCount, formatBytes(totalRecv), formatBytes(totalResp))

	n := len(snapshots)
	if n > c.topN {
		n = c.topN
	}
	for i := 0; i < n; i++ {
		s := snapshots[i]
		logx.Infof("[grpcstats]   %s count=%d req=%s resp=%s avg_lat=%v max_lat=%v max_req=%s",
			s.FullMethod, s.RecvCount,
			formatBytes(s.RecvBytes), formatBytes(s.RespBytes),
			s.LatencyAvg, s.MaxLatency,
			formatBytes(s.MaxReqSize))
	}

	if len(snapshots) > c.topN {
		logx.Infof("[grpcstats]   ... and %d more methods", len(snapshots)-c.topN)
	}
}

func formatBytes(b int64) string {
	switch {
	case b >= 1<<30:
		return strconv.FormatFloat(float64(b)/float64(1<<30), 'f', 1, 64) + "GB"
	case b >= 1<<20:
		return strconv.FormatFloat(float64(b)/float64(1<<20), 'f', 1, 64) + "MB"
	case b >= 1<<10:
		return strconv.FormatFloat(float64(b)/float64(1<<10), 'f', 1, 64) + "KB"
	default:
		return strconv.FormatInt(b, 10) + "B"
	}
}

// ShortMethod extracts the method name from a full gRPC method path like
// "/package.Service/Method" → "Service/Method".
func ShortMethod(fullMethod string) string {
	if idx := strings.LastIndex(fullMethod, "/"); idx >= 0 {
		parts := strings.SplitN(fullMethod[1:], "/", 2)
		if len(parts) == 2 {
			svc := parts[0]
			if dot := strings.LastIndex(svc, "."); dot >= 0 {
				svc = svc[dot+1:]
			}
			return svc + "/" + parts[1]
		}
	}
	return fullMethod
}
