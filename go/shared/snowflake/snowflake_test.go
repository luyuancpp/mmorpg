package snowflake

import (
	"sync"
	"testing"
)

func TestGenerate_Unique(t *testing.T) {
	n := NewNode(0)
	const count = 10000
	seen := make(map[uint64]bool, count)
	for i := 0; i < count; i++ {
		id := n.Generate()
		if seen[id] {
			t.Fatalf("duplicate ID %d at iteration %d", id, i)
		}
		seen[id] = true
	}
}

func TestGenerate_Monotonic(t *testing.T) {
	n := NewNode(0)
	prev := uint64(0)
	for i := 0; i < 1000; i++ {
		id := n.Generate()
		if id <= prev {
			t.Fatalf("ID not monotonic: prev=%d, got=%d at iteration %d", prev, id, i)
		}
		prev = id
	}
}

func TestGenerate_EmbeddedNodeID(t *testing.T) {
	nodeID := uint64(42)
	n := NewNode(nodeID)
	id := n.Generate()

	extracted := (id >> nodeShift) & NodeMask
	if extracted != nodeID {
		t.Fatalf("expected nodeID %d in ID, got %d", nodeID, extracted)
	}
}

func TestGenerate_DifferentNodes_NoDuplicates(t *testing.T) {
	n1 := NewNode(1)
	n2 := NewNode(2)
	seen := make(map[uint64]bool)
	for i := 0; i < 1000; i++ {
		id1 := n1.Generate()
		id2 := n2.Generate()
		if seen[id1] {
			t.Fatalf("duplicate from node1: %d", id1)
		}
		if seen[id2] {
			t.Fatalf("duplicate from node2: %d", id2)
		}
		seen[id1] = true
		seen[id2] = true
	}
}

func TestGenerate_ConcurrentSafety(t *testing.T) {
	n := NewNode(0)
	const goroutines = 8
	const perGoroutine = 5000

	var mu sync.Mutex
	seen := make(map[uint64]bool, goroutines*perGoroutine)

	var wg sync.WaitGroup
	wg.Add(goroutines)
	for g := 0; g < goroutines; g++ {
		go func() {
			defer wg.Done()
			ids := make([]uint64, perGoroutine)
			for i := 0; i < perGoroutine; i++ {
				ids[i] = n.Generate()
			}
			mu.Lock()
			defer mu.Unlock()
			for _, id := range ids {
				if seen[id] {
					t.Errorf("duplicate ID %d", id)
				}
				seen[id] = true
			}
		}()
	}
	wg.Wait()
}

func TestNewNode_PanicsOnOverflow(t *testing.T) {
	defer func() {
		if r := recover(); r == nil {
			t.Fatal("expected panic for oversized nodeID")
		}
	}()
	NewNode(NodeMask + 1)
}

func TestNewNode_MaxNodeID(t *testing.T) {
	n := NewNode(NodeMask)
	id := n.Generate()
	extracted := (id >> nodeShift) & NodeMask
	if extracted != NodeMask {
		t.Fatalf("expected nodeID %d, got %d", NodeMask, extracted)
	}
}
