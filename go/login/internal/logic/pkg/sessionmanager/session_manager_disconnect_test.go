package sessionmanager

import (
	"context"
	"errors"
	"testing"

	plpb "proto/player_locator"
	commonbase "proto/common/base"

	"google.golang.org/grpc"
)

// fakePlayerLocatorClient is the smallest implementation of
// plpb.PlayerLocatorClient that lets us steer SetDisconnecting outcomes
// in tests without standing up a real gRPC server.
//
// It records the last call so each test can assert on the request
// payload (player_id, session_id, lease_ttl) and choose what to return.
// All other methods are deliberately stubbed with panic — we only
// exercise SetDisconnecting here, and a panic surfaces a future test
// that strays from the contract under test.
type fakePlayerLocatorClient struct {
	lastSetDisconnecting *plpb.SetDisconnectingRequest
	setDisconnectErr     error
	setDisconnectCalls   int
}

func (f *fakePlayerLocatorClient) SetLocation(context.Context, *plpb.PlayerLocation, ...grpc.CallOption) (*commonbase.Empty, error) {
	panic("SetLocation not used in this test")
}
func (f *fakePlayerLocatorClient) GetLocation(context.Context, *plpb.PlayerId, ...grpc.CallOption) (*plpb.PlayerLocation, error) {
	panic("GetLocation not used in this test")
}
func (f *fakePlayerLocatorClient) MarkOffline(context.Context, *plpb.PlayerId, ...grpc.CallOption) (*commonbase.Empty, error) {
	panic("MarkOffline not used in this test")
}
func (f *fakePlayerLocatorClient) SetSession(context.Context, *plpb.SetSessionRequest, ...grpc.CallOption) (*commonbase.Empty, error) {
	panic("SetSession not used in this test")
}
func (f *fakePlayerLocatorClient) GetSession(context.Context, *plpb.GetSessionRequest, ...grpc.CallOption) (*plpb.GetSessionResponse, error) {
	panic("GetSession not used in this test")
}
func (f *fakePlayerLocatorClient) SetDisconnecting(_ context.Context, in *plpb.SetDisconnectingRequest, _ ...grpc.CallOption) (*commonbase.Empty, error) {
	f.setDisconnectCalls++
	f.lastSetDisconnecting = in
	return &commonbase.Empty{}, f.setDisconnectErr
}
func (f *fakePlayerLocatorClient) Reconnect(context.Context, *plpb.ReconnectRequest, ...grpc.CallOption) (*plpb.ReconnectResponse, error) {
	panic("Reconnect not used in this test")
}

// TestSetSessionDisconnecting_PassesPlayerIdAndSessionId is the regression
// guard for the cpp-gate fix in this commit series: gate's
// HandleConnectionDisconnection now copies SessionInfo.playerId into
// SessionDetails, so login's markPlayerSessionDisconnecting() actually
// reaches this RPC (instead of early-returning on playerID == 0). If
// anyone later changes the wire shape so player_id stops travelling,
// this test will fail and the deprecation/lease leak resurfaces.
//
// We pin the lease at 30s because the doc and the disconnect-cleanup
// timing both depend on that exact value (see
// docs/design/player_login_flow.md §2 Disconnect: "30s reconnect window").
func TestSetSessionDisconnecting_PassesPlayerIdAndSessionId(t *testing.T) {
	mock := &fakePlayerLocatorClient{}
	if err := SetSessionDisconnecting(context.Background(), mock, 12345, 678); err != nil {
		t.Fatalf("expected nil error, got %v", err)
	}
	if mock.setDisconnectCalls != 1 {
		t.Fatalf("expected exactly one SetDisconnecting call, got %d", mock.setDisconnectCalls)
	}
	got := mock.lastSetDisconnecting
	if got.PlayerId != 12345 {
		t.Errorf("PlayerId mismatch: got %d, want 12345", got.PlayerId)
	}
	if got.SessionId != 678 {
		t.Errorf("SessionId mismatch: got %d, want 678", got.SessionId)
	}
	if got.LeaseTtlSeconds != 30 {
		t.Errorf("LeaseTtlSeconds: got %d, want 30 (matches disconnect-window doc)", got.LeaseTtlSeconds)
	}
}

// TestSetSessionDisconnecting_WrapsRpcError ensures upstream gRPC errors
// surface to the caller — login's markPlayerSessionDisconnecting() logs
// these but does not panic; the helper at this layer must propagate
// faithfully so the upper layer can log AND continue cleanup of other
// state (loginsession.Cleanup happens before this call, by design).
func TestSetSessionDisconnecting_WrapsRpcError(t *testing.T) {
	mock := &fakePlayerLocatorClient{
		setDisconnectErr: errors.New("locator unavailable"),
	}
	err := SetSessionDisconnecting(context.Background(), mock, 1, 2)
	if err == nil {
		t.Fatal("expected error, got nil")
	}
	// Wrap message comes from session_manager.go — check the prefix so
	// future formatting tweaks don't break the test brittlely.
	if got := err.Error(); got == "" || !contains(got, "player_locator SetDisconnecting") {
		t.Errorf("error message did not include wrap prefix; got %q", got)
	}
}

func contains(s, sub string) bool {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return true
		}
	}
	return false
}
