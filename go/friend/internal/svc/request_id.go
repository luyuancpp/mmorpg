package svc

import (
	"errors"
	"time"

	dspb "proto/data_service"
	"shared/idsegment"
)

// RequestIDBizTag 与 data_service 的四处启动播种清单保持一致。申请代次不从
// friend 节点编号或墙钟推导，服务重启、扩副本和时钟回拨不能重复发号。
const RequestIDBizTag = "friend_request"

func NewRequestIDSegment(client dspb.DataServiceClient) (*idsegment.Client, error) {
	if client == nil {
		return nil, errors.New("好友申请号源需要 data_service 客户端")
	}
	return idsegment.New(idsegment.Adapt(client.AllocateIdSegment,
		func(tag string, step uint32) *dspb.AllocateIdSegmentRequest {
			return &dspb.AllocateIdSegmentRequest{BizTag: tag, Step: step}
		}), idsegment.Options{
		BizTag: RequestIDBizTag, Step: 1000, MinStep: 100, MaxStep: 100000,
		FetchTimeout: 1500 * time.Millisecond,
	})
}
