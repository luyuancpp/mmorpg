package main

// 仅验收root拥有的真实C++ Battle节点；没有fake RPC服务器，也不启动/停止节点。
// 必须先编译测试二进制，再取得Match号源产生且尚未dispatch的专属Create原件。
// -mod=readonly使用../go/proto正式PB，不能以旧vendor契约替代本轮结果。
import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"errors"
	"fmt"
	"net"
	"os"
	"strconv"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/protobuf/proto"

	"proto/battle"
	"robot/generated/pb/game"
	tiptable "shared/generated/pb/table"
)

const ownedObserverWireTimeout = 3 * time.Second

type ownedObserverWire struct {
	t       *testing.T
	root    *testing.T
	ctx     context.Context
	client  battle.BattleNodeClient
	create  *battle.CreateBattleRequest
	original *battle.ObserverExitOriginal
}

// 原件只从root显式提供的二进制PB读入；日志只记SHA，不打印票据、私有账号或原件正文。
func readOwnedObserverProto(t *testing.T, env string, message proto.Message) {
	t.Helper()
	path := os.Getenv(env)
	if path == "" {
		t.Fatalf("required owned fixture %s is missing", env)
	}
	raw, err := os.ReadFile(path)
	if err != nil || len(raw) == 0 || len(raw) > 2*1024*1024 {
		t.Fatalf("read owned fixture %s: bytes=%d err=%v", env, len(raw), err)
	}
	if err := proto.Unmarshal(raw, message); err != nil {
		t.Fatalf("decode official PB %s: %v", env, err)
	}
	if len(message.ProtoReflect().GetUnknown()) != 0 {
		t.Fatalf("official PB %s has unknown fields; rebind current generated contract", env)
	}
	t.Logf("owned_fixture env=%s sha256=%x", env, sha256.Sum256(raw))
}

func observerWireUUID(t *testing.T) string {
	t.Helper()
	var id [16]byte
	if _, err := rand.Read(id[:]); err != nil {
		t.Fatalf("fresh test operation UUID: %v", err)
	}
	id[6] = (id[6] & 15) | 64
	id[8] = (id[8] & 63) | 128
	return fmt.Sprintf("%x-%x-%x-%x-%x", id[0:4], id[4:6], id[6:8], id[8:10], id[10:16])
}

func ownedLoopbackEndpoint(t *testing.T, value string) {
	t.Helper()
	host, port, err := net.SplitHostPort(value)
	n, parseErr := strconv.Atoi(port)
	if err != nil || net.ParseIP(host) == nil || !net.ParseIP(host).Equal(net.ParseIP("127.0.0.1")) || parseErr != nil || n < 1 || n > 65535 {
		t.Fatal("owned Battle endpoint must be numeric 127.0.0.1 with a valid explicit port")
	}
}

func (w *ownedObserverWire) rpcContext() (context.Context, context.CancelFunc) {
	return context.WithTimeout(w.ctx, ownedObserverWireTimeout)
}

func (w *ownedObserverWire) fresh() *battle.AddObserverRequest {
	w.t.Helper()
	request := proto.Clone(w.original.GetAdd()).(*battle.AddObserverRequest)
	request.BattleId = w.create.GetBattleId()
	request.TargetInstanceId = w.create.GetTargetInstanceId()
	request.SpectatorOperationId = observerWireUUID(w.t)
	// 测试新意图首次捕获后保持不变；Remove和重放永远复用这份deadline。
	request.AdmitBeforeMs = uint64(time.Now().UnixMilli()) + 28000
	w.root.Cleanup(func() {
		ctx, cancel := context.WithTimeout(context.Background(), ownedObserverWireTimeout)
		defer cancel()
		reply, err := w.client.RemoveObserver(ctx, removeOwnedObserver(request))
		if err != nil || !reply.GetCancelled() || reply.GetSpectatorOperationId() != request.GetSpectatorOperationId() || reply.GetTargetInstanceId() != request.GetTargetInstanceId() {
			w.root.Errorf("owned operation cleanup lacks exact cancellation proof: err=%v", err)
		}
	})
	return request
}

func removeOwnedObserver(original *battle.AddObserverRequest) *battle.RemoveObserverRequest {
	return &battle.RemoveObserverRequest{
		BattleId: original.GetBattleId(), ObserverPlayerId: original.GetObserverPlayerId(),
		SpectatorOperationId: original.GetSpectatorOperationId(), TargetInstanceId: original.GetTargetInstanceId(),
		AdmitBeforeMs: original.GetAdmitBeforeMs(), Reason: "owned-observer-wire-contract",
	}
}

func (w *ownedObserverWire) add(original *battle.AddObserverRequest, expectedTip uint32) {
	w.t.Helper()
	ctx, cancel := w.rpcContext()
	defer cancel()
	reply, err := w.client.AddObserver(ctx, original)
	if err != nil || reply.GetErrorMessage().GetId() != expectedTip {
		w.t.Fatalf("AddObserver tip=%d want=%d rpc_err=%v", reply.GetErrorMessage().GetId(), expectedTip, err)
	}
}

func (w *ownedObserverWire) remove(original *battle.AddObserverRequest, cancelled bool) {
	w.t.Helper()
	ctx, cancel := w.rpcContext()
	defer cancel()
	reply, err := w.client.RemoveObserver(ctx, removeOwnedObserver(original))
	if err != nil || reply.GetCancelled() != cancelled {
		w.t.Fatalf("RemoveObserver proof=%t want=%t rpc_err=%v", reply.GetCancelled(), cancelled, err)
	}
	if cancelled && (reply.GetSpectatorOperationId() != original.GetSpectatorOperationId() || reply.GetTargetInstanceId() != original.GetTargetInstanceId()) {
		w.t.Fatal("RemoveObserver cancellation proof is not bound to the original operation and instance")
	}
}

func (w *ownedObserverWire) query(original *battle.AddObserverRequest, expected battle.ObserverAdmissionState) {
	w.t.Helper()
	ctx, cancel := w.rpcContext()
	defer cancel()
	reply, err := w.client.QueryObserver(ctx, &battle.QueryObserverRequest{
		BattleId: original.GetBattleId(), ObserverPlayerId: original.GetObserverPlayerId(),
		SpectatorOperationId: original.GetSpectatorOperationId(), TargetInstanceId: original.GetTargetInstanceId(),
	})
	if err != nil || reply.GetState() != expected {
		w.t.Fatalf("QueryObserver state=%s want=%s rpc_err=%v", reply.GetState(), expected, err)
	}
	if expected != battle.ObserverAdmissionState_OBSERVER_ADMISSION_UNKNOWN &&
		(reply.GetSpectatorOperationId() != original.GetSpectatorOperationId() || reply.GetTargetInstanceId() != original.GetTargetInstanceId()) {
		w.t.Fatal("QueryObserver response is not bound to the original operation and instance")
	}
}

func (w *ownedObserverWire) ticket(original *battle.AddObserverRequest) *battle.BattleAssignedS2C {
	w.t.Helper()
	ctx, cancel := w.rpcContext()
	defer cancel()
	reply, err := w.client.IssueBattleTicket(ctx, &battle.IssueBattleTicketRequest{
		BattleId: original.GetBattleId(), PlayerId: original.GetObserverPlayerId(),
		SpectatorOperationId: original.GetSpectatorOperationId(), TargetInstanceId: original.GetTargetInstanceId(),
	})
	if err != nil || reply.GetErrorMessage().GetId() != 0 || reply.GetAssignment() == nil {
		w.t.Fatalf("IssueBattleTicket tip=%d rpc_err=%v", reply.GetErrorMessage().GetId(), err)
	}
	assigned := reply.GetAssignment()
	var payload battle.BattleTicketPayload
	if err := proto.Unmarshal(assigned.GetTokenPayload(), &payload); err != nil {
		w.t.Fatalf("decode actual signed BattleTicketPayload: %v", err)
	}
	if len(assigned.GetTokenSignature()) == 0 || payload.GetBattleId() != original.GetBattleId() ||
		payload.GetPlayerId() != original.GetObserverPlayerId() || payload.GetBattleNodeId() == 0 ||
		payload.GetBattleInstanceId() != original.GetTargetInstanceId() || payload.GetSpectatorOperationId() != original.GetSpectatorOperationId() ||
		payload.GetRole() != battle.EBattleTicketRole_BATTLE_TICKET_ROLE_OBSERVER || assigned.GetRole() != payload.GetRole() ||
		assigned.GetBattleId() != payload.GetBattleId() || assigned.GetExpireAtMs() != payload.GetExpireAtMs() {
		w.t.Fatal("actual issued ticket lacks complete original observer/operation/instance/role binding")
	}
	ownedLoopbackEndpoint(w.t, net.JoinHostPort(assigned.GetHost(), strconv.FormatUint(uint64(assigned.GetPort()), 10)))
	return assigned
}

func (w *ownedObserverWire) dial(assigned *battle.BattleAssignedS2C) *ownedWireDirect {
	w.t.Helper()
	ctx, cancel := w.rpcContext()
	defer cancel()
	conn, err := dialOwnedWireDirect(ctx, assigned)
	if err != nil {
		w.t.Fatalf("real Battle direct handshake: %v", err)
	}
	w.root.Cleanup(conn.Close)
	return conn
}

func (w *ownedObserverWire) state(conn *ownedWireDirect) {
	w.t.Helper()
	reply, err := conn.call(game.BattleClientPlayerGetBattleStateMessageId, &battle.GetBattleStateRequest{BattleId: w.create.GetBattleId()})
	if err != nil {
		w.t.Fatalf("current direct GetBattleState: %v", err)
	}
	var state battle.BattleStateS2C
	if reply.GetMessageId() != game.BattleClientPlayerGetBattleStateMessageId || proto.Unmarshal(reply.GetSerializedMessage(), &state) != nil || state.GetBattleId() != w.create.GetBattleId() {
		w.t.Fatal("current direct GetBattleState did not return the live owned room")
	}
}

func (w *ownedObserverWire) stale(conn *ownedWireDirect, original *battle.AddObserverRequest) {
	w.t.Helper()
	if err := conn.staleClosed(w.create.GetBattleId()); err != nil {
		w.t.Fatalf("old direct Get/Stop lacks positive closure or bound rejection proof: %v", err)
	}
	w.query(original, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ACTIVE)
}

func TestOwnedBattleObserverRPCContract(t *testing.T) {
	if os.Getenv("MMORPG_BATTLE_WIRE_REQUIRE") != "1" {
		t.Skip("requires root-owned live C++ Battle fixture; required acceptance sets MMORPG_BATTLE_WIRE_REQUIRE=1")
	}
	endpoint := os.Getenv("MMORPG_BATTLE_WIRE_RPC")
	ownedLoopbackEndpoint(t, endpoint)
	create := &battle.CreateBattleRequest{}
	original := &battle.ObserverExitOriginal{}
	readOwnedObserverProto(t, "MMORPG_BATTLE_WIRE_CREATE_PB", create)
	readOwnedObserverProto(t, "MMORPG_BATTLE_WIRE_OBSERVER_PB", original)
	if create.GetBattleId() == 0 || create.GetPlacementOperationId() == "" || create.GetTargetInstanceId() == "" || create.GetRequiredPhysicalRegion() == 0 || len(create.GetPlayers()) < 2 ||
		original.GetPlayerId() == 0 || original.GetOwnerAccount() == "" || original.GetOwnerRevision() == 0 || original.GetAdd() == nil ||
		original.GetAdd().GetObserverPlayerId() != original.GetPlayerId() || original.GetAdd().GetTargetInstanceId() != create.GetTargetInstanceId() ||
		original.GetInstanceId() != create.GetTargetInstanceId() || original.GetAdd().GetRouting().GetSessionId() == 0 ||
		original.GetAdd().GetRouting().GetGateNodeId() == 0 || original.GetAdd().GetRouting().GetGateInstanceId() == "" || original.GetAdd().GetRouting().GetZoneId() == 0 {
		t.Fatal("owned fixture lacks complete original Create/observer identity/routing/instance")
	}
	for _, player := range create.GetPlayers() {
		if player.GetPlayerId() == original.GetPlayerId() { t.Fatal("dedicated observer is a participant") }
	}
	now := uint64(time.Now().UnixMilli())
	if create.GetAdmitBeforeMs() <= now+5000 || create.GetDeadlineMs() <= now+120000 {
		t.Fatal("owned original Create is too old; prepare a new Match-owned original after compiling the test binary; do not rewrite a persisted deadline")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 120*time.Second)
	t.Cleanup(cancel)
	cc, err := grpc.NewClient(endpoint, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil { t.Fatalf("connect owned Battle RPC: %v", err) }
	t.Cleanup(func() { if err := cc.Close(); err != nil { t.Errorf("close owned RPC client: %v", err) } })
	w := &ownedObserverWire{t: t, root: t, ctx: ctx, client: battle.NewBattleNodeClient(cc), create: create, original: original}
	invalid := uint32(tiptable.CommonError_kInvalidParameter)
	var current, next *battle.AddObserverRequest
	var oldTicket *battle.BattleAssignedS2C
	var firstConn, secondConn, thirdConn *ownedWireDirect

	if !t.Run("CancelBeforeRoomAndLateAdd", func(t *testing.T) {
		w.t = t
		queryCtx, done := w.rpcContext()
		state, err := w.client.QueryBattleCreation(queryCtx, &battle.QueryBattleCreationRequest{BattleId: create.GetBattleId(), PlacementOperationId: create.GetPlacementOperationId(), TargetInstanceId: create.GetTargetInstanceId()})
		done()
		if err != nil || state.GetState() != battle.BattleCreationState_BATTLE_CREATION_ABSENT {
			t.Fatalf("owned room must not exist before this contract: state=%s err=%v", state.GetState(), err)
		}
		cancelled := w.fresh()
		w.remove(cancelled, true)
		w.query(cancelled, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ABSENT)
		createCtx, done := w.rpcContext()
		reply, err := w.client.CreateBattle(createCtx, create)
		done()
		if err != nil || reply.GetErrorMessage().GetId() != 0 || reply.GetBattleId() != create.GetBattleId() {
			t.Fatalf("Create original dedicated room: tip=%d err=%v", reply.GetErrorMessage().GetId(), err)
		}
		// 只销毁本测试首次创建、原placement/instance完全相同的专属房间。
		w.root.Cleanup(func() {
			cleanupCtx, done := context.WithTimeout(context.Background(), ownedObserverWireTimeout)
			defer done()
			_, err := w.client.DestroyBattle(cleanupCtx, &battle.DestroyBattleRequest{BattleId: create.GetBattleId(), PlacementOperationId: create.GetPlacementOperationId(), TargetInstanceId: create.GetTargetInstanceId(), Reason: "owned-observer-wire-cleanup"})
			if err != nil { w.root.Errorf("destroy dedicated original room: %v", err) }
		})
		w.add(cancelled, invalid)
		w.query(cancelled, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ABSENT)
	}) { return }
	if !t.Run("ExactReplayAndChangedPayload", func(t *testing.T) {
		w.t = t; current = w.fresh(); w.add(current, 0); w.add(current, 0)
		changed := proto.Clone(current).(*battle.AddObserverRequest)
		changed.ObserverName += "-changed"
		w.add(changed, invalid)
		w.query(current, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ACTIVE)
	}) { return }
	if !t.Run("DeadlineAndInstanceFailClosed", func(t *testing.T) {
		w.t = t
		wrong := proto.Clone(current).(*battle.AddObserverRequest)
		wrong.TargetInstanceId = observerWireUUID(t)
		w.add(wrong, invalid); w.remove(wrong, false); w.query(wrong, battle.ObserverAdmissionState_OBSERVER_ADMISSION_UNKNOWN)
		wrong = proto.Clone(current).(*battle.AddObserverRequest); wrong.AdmitBeforeMs++
		w.remove(wrong, false)
		for _, deadline := range []uint64{uint64(time.Now().UnixMilli()) - 1, uint64(time.Now().UnixMilli()) + 60000} {
			bad := proto.Clone(current).(*battle.AddObserverRequest); bad.SpectatorOperationId = observerWireUUID(t); bad.AdmitBeforeMs = deadline
			w.add(bad, invalid)
		}
		w.query(current, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ACTIVE)
	}) { return }
	if !t.Run("ActualSignedTicketAndDirectState", func(t *testing.T) {
		w.t = t; oldTicket = w.ticket(current); firstConn = w.dial(oldTicket); w.state(firstConn)
	}) { return }
	if !t.Run("ReconnectRejectsOldConnectionGetAndStop", func(t *testing.T) {
		w.t = t; secondConn = w.dial(w.ticket(current)); w.state(secondConn); w.stale(firstConn, current); w.state(secondConn)
	}) { return }
	if !t.Run("OldRemoveAndTicketCannotTakeNewOperation", func(t *testing.T) {
		w.t = t; w.remove(current, true); next = w.fresh(); w.add(next, 0)
		w.remove(current, true); w.query(next, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ACTIVE)
		// 票据HMAC真实有效但原operation已被撤销，必须握手失败。
		oldCtx, done := w.rpcContext()
		staleConn, err := dialOwnedWireDirect(oldCtx, oldTicket)
		done()
		if staleConn != nil { staleConn.Close() }
		var rejected *ownedWireTicketRejected
		if !errors.As(err, &rejected) { t.Fatalf("old signed ticket lacks an actual explicit rejection: %v", err) }
		thirdConn = w.dial(w.ticket(next)); w.state(thirdConn); w.stale(secondConn, next); w.state(thirdConn)
	}) { return }
	if !t.Run("CurrentStopNeedsOriginalCancellationProof", func(t *testing.T) {
		w.t = t
		reply, err := thirdConn.call(game.BattleClientPlayerStopWatchBattleMessageId, &battle.StopWatchBattleRequest{BattleId: create.GetBattleId()})
		var stopped battle.StopWatchBattleResponse
		if err != nil || reply.GetMessageId() != game.BattleClientPlayerStopWatchBattleMessageId || proto.Unmarshal(reply.GetSerializedMessage(), &stopped) != nil || stopped.GetErrorMessage().GetId() != 0 {
			t.Fatalf("current direct StopWatchBattle: err=%v", err)
		}
		w.query(next, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ABSENT)
		w.remove(next, true)
		w.add(next, invalid)
		w.query(next, battle.ObserverAdmissionState_OBSERVER_ADMISSION_ABSENT)
	}) { return }
}
