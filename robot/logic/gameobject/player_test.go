package gameobject

import "testing"

func TestGetRandomEntityFallsBackToSelf(t *testing.T) {
	p := &Player{ID: 1001}
	p.SetEntityID(4242)

	if got := p.GetRandomEntity(); got != 4242 {
		t.Fatalf("expected self entity fallback 4242, got %d", got)
	}
}

func TestSceneStateRoundTrip(t *testing.T) {
	p := &Player{ID: 1001}
	p.SetSceneInfo(9001, 1001)

	if got := p.GetSceneID(); got != 9001 {
		t.Fatalf("expected scene id 9001, got %d", got)
	}
	if got := p.GetSceneConfigID(); got != 1001 {
		t.Fatalf("expected scene config id 1001, got %d", got)
	}
}
