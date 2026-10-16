package svc

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
	"testing"

	dspb "proto/data_service"
	"shared/idsegment"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
)

type requestSegmentService struct {
	dspb.DataServiceClient
	next   atomic.Uint64
	badTag atomic.Bool
	err    error
}

func (f *requestSegmentService) AllocateIdSegment(_ context.Context, in *dspb.AllocateIdSegmentRequest, _ ...grpc.CallOption) (*dspb.AllocateIdSegmentResponse, error) {
	if in.GetBizTag() != RequestIDBizTag {
		f.badTag.Store(true)
	}
	if f.err != nil {
		return nil, f.err
	}
	hi := f.next.Add(uint64(in.GetStep())) + 1
	return &dspb.AllocateIdSegmentResponse{Lo: hi - uint64(in.GetStep()), Hi: hi}, nil
}

func TestRequestIDSegment_MultipleReplicasAndClose(t *testing.T) {
	service := &requestSegmentService{}
	left, err := NewRequestIDSegment(service)
	require.NoError(t, err)
	right, err := NewRequestIDSegment(service)
	require.NoError(t, err)
	t.Cleanup(left.Close)
	t.Cleanup(right.Close)
	seen := sync.Map{}
	var wg sync.WaitGroup
	for _, client := range []*idsegment.Client{left, right} {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for i := 0; i < 1500; i++ {
				id, err := client.Next(context.Background())
				if err != nil {
					t.Errorf("号段发号失败: %v", err)
					return
				}
				if id == 0 {
					t.Error("发出零代次")
				}
				if _, loaded := seen.LoadOrStore(id, true); loaded {
					t.Errorf("副本发出重复代次 %d", id)
				}
			}
		}()
	}
	wg.Wait()
	assert.False(t, service.badTag.Load())
	left.Close()
	_, err = left.Next(context.Background())
	assert.ErrorIs(t, err, idsegment.ErrClosed)
}

func TestRequestIDSegment_SourceFailureNeverFallsBack(t *testing.T) {
	_, err := NewRequestIDSegment(nil)
	require.Error(t, err)
	client, err := NewRequestIDSegment(&requestSegmentService{err: errors.New("领段失败")})
	require.NoError(t, err)
	t.Cleanup(client.Close)
	id, err := client.Next(context.Background())
	assert.Zero(t, id)
	assert.ErrorIs(t, err, idsegment.ErrSegmentUnavailable)
}
