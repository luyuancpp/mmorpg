package pregate

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"sort"
	"time"

	"login/internal/config"
	"login/internal/svc"
	commonpb "proto/common/base"
	login "proto/login"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

const tokenTTLSeconds = 60 // token valid for 60 seconds

// AssignGate picks the least-loaded Gate for the requested zone, signs a
// one-time HMAC connection token, and returns the gate address + token.
func AssignGate(ctx context.Context, svcCtx *svc.ServiceContext, req *login.AssignGateRequest) (*login.AssignGateResponse, error) {
	nodes, err := svcCtx.GateWatcher.FetchAllNodes()
	if err != nil {
		logx.Errorf("AssignGate: failed to fetch gate nodes: %v", err)
		return &login.AssignGateResponse{Error: "no gate nodes available"}, nil
	}

	// Filter by zone_id if specified (0 means any zone)
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
		if req.ZoneId != 0 && n.ZoneId != req.ZoneId {
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
		return &login.AssignGateResponse{Error: "no gate available for requested zone"}, nil
	}

	// Pick the least-loaded gate
	sort.Slice(candidates, func(i, j int) bool {
		return candidates[i].playerCount < candidates[j].playerCount
	})
	best := candidates[0]

	// Build and sign the connection token
	expireTS := time.Now().Unix() + tokenTTLSeconds

	payload := &commonpb.GateTokenPayload{
		GateNodeId:      best.nodeId,
		ZoneId:          best.zoneId,
		ExpireTimestamp:  expireTS,
	}
	payloadBytes, err := proto.Marshal(payload)
	if err != nil {
		logx.Errorf("AssignGate: marshal token payload: %v", err)
		return &login.AssignGateResponse{Error: "internal error"}, nil
	}

	secret := config.AppConfig.GateTokenSecret
	signature := signHMAC(secret, payloadBytes)

	return &login.AssignGateResponse{
		Ip:             best.ip,
		Port:           best.port,
		TokenPayload:   payloadBytes,
		TokenSignature: signature,
		TokenDeadline:  expireTS,
	}, nil
}

// signHMAC computes HMAC-SHA256(secret, data) and returns the hex-encoded string as bytes.
// Gate-side uses HmacSha256Hex which also produces lowercase hex, so this matches.
func signHMAC(secret string, data []byte) []byte {
	mac := hmac.New(sha256.New, []byte(secret))
	mac.Write(data)
	return []byte(hex.EncodeToString(mac.Sum(nil)))
}
