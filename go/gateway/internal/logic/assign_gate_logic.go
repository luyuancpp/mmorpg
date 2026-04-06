package logic

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"sort"
	"time"

	"gateway/internal/svc"
	"gateway/internal/types"
	commonpb "proto/common/base"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

const tokenTTLSeconds = 60 // token valid for 60 seconds

type AssignGateLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
}

func NewAssignGateLogic(ctx context.Context, svcCtx *svc.ServiceContext) *AssignGateLogic {
	return &AssignGateLogic{ctx: ctx, svcCtx: svcCtx}
}

func (l *AssignGateLogic) AssignGate(req *types.AssignGateRequest) (*types.AssignGateResponse, error) {
	nodes, err := l.svcCtx.GateWatcher.FetchAllNodes()
	if err != nil {
		logx.Errorf("AssignGate: failed to fetch gate nodes: %v", err)
		return &types.AssignGateResponse{Error: "no gate nodes available"}, nil
	}

	type candidate struct {
		nodeId      uint32
		ip          string
		port        uint32
		playerCount uint32
		zoneId      uint32
	}
	var candidates []candidate
	for _, n := range nodes {
		if n.Endpoint == nil {
			continue
		}
		if req.ZoneID != 0 && n.ZoneId != req.ZoneID {
			continue
		}
		candidates = append(candidates, candidate{
			nodeId:      n.NodeId,
			ip:          n.Endpoint.Ip,
			port:        n.Endpoint.Port,
			playerCount: n.PlayerCount,
			zoneId:      n.ZoneId,
		})
	}

	if len(candidates) == 0 {
		return &types.AssignGateResponse{Error: "no gate available for requested zone"}, nil
	}

	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].playerCount < candidates[j].playerCount
	})
	best := candidates[0]

	expireTS := time.Now().Unix() + tokenTTLSeconds
	payload := &commonpb.GateTokenPayload{
		GateNodeId:     best.nodeId,
		ZoneId:         best.zoneId,
		ExpireTimestamp: expireTS,
	}
	payloadBytes, err := proto.Marshal(payload)
	if err != nil {
		logx.Errorf("AssignGate: marshal token payload: %v", err)
		return &types.AssignGateResponse{Error: "internal error"}, nil
	}

	secret := l.svcCtx.Config.Gate.TokenSecret
	signature := signHMAC(secret, payloadBytes)

	return &types.AssignGateResponse{
		GateIP:         best.ip,
		GatePort:       best.port,
		TokenPayload:   payloadBytes,
		TokenSignature: signature,
		TokenDeadline:  expireTS,
	}, nil
}

func signHMAC(secret string, data []byte) []byte {
	mac := hmac.New(sha256.New, []byte(secret))
	mac.Write(data)
	return []byte(hex.EncodeToString(mac.Sum(nil)))
}
