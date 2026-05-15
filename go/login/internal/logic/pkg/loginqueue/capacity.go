// capacity.go — per-zone admission ceiling computation.
//
// Two questions this file answers:
//
//  1. How many players can zone Z host concurrently? (zoneCapacity)
//  2. How many free slots does zone Z have RIGHT NOW? (freeSlots)
//
// The capacity number is config-first, with a fallback derived from the
// gates' observed PlayerCount * SoftCapMultiplier. Operators should pin a
// real number in YAML for production; the multiplier exists so a freshly
// deployed zone (PlayerCount=0 across all gates) doesn't permanently lock
// new players out of the queue.
//
// Free-slots calculation deliberately includes admitted-but-not-connected
// players (the dispatcher's `admitted:zone:{zoneId}` set):
//
//	freeSlots = capacity - online - admitted
//
// Without the `admitted` term, a single dispatcher tick would over-issue
// gate tokens by a factor of (admitted / dispatch_interval) until the next
// PlayerCount refresh propagates from gate → etcd → GateWatcher.
package loginqueue

import (
	"context"
	"fmt"
	"strconv"
)

// CapacityProvider exposes the inputs capacity calculation needs. The
// concrete implementation in svc wires GateWatcher + ZoneCapacityOverride
// + the queue's admittedSetKey count.
type CapacityProvider interface {
	// CandidatesForZone returns all healthy gates serving zoneID, in
	// arbitrary order. PickAndSignGateToken sorts by load.
	CandidatesForZone(ctx context.Context, zoneID uint32) ([]GateCandidate, error)
	// ZoneCapacity is the configured ceiling for zoneID, or 0 when
	// unconfigured (caller falls back to softCap formula).
	ZoneCapacity(zoneID uint32) uint32
}

// FreeSlots reports how many additional players the zone can accept right
// now. May return 0 when the answer is "queue must hold them".
//
// The formula is:
//
//	online    = sum(candidate.PlayerCount for candidate in zone)
//	cap       = ZoneCapacity(zone) || ceil(max(online, 1) * softCapMultiplier)
//	admitted  = SCARD admitted:zone:{zone}
//	free      = max(0, cap - online - admitted)
//
// We use max(online, 1) in the fallback so a brand-new zone (online=0)
// gets a non-zero capacity from the multiplier. Pinning ZoneCapacity in
// config remains the strongly recommended path for production.
func FreeSlots(
	ctx context.Context,
	provider CapacityProvider,
	queue *Queue,
	zoneID uint32,
	softCapMultiplier float64,
) (free uint32, capacity uint32, online uint32, admitted uint32, err error) {
	candidates, cErr := provider.CandidatesForZone(ctx, zoneID)
	if cErr != nil {
		err = fmt.Errorf("candidates: %w", cErr)
		return
	}
	if len(candidates) == 0 {
		// No gates → no capacity. Caller surfaces this as an "error" rather
		// than queueing, since the queue draining requires a healthy gate.
		return 0, 0, 0, 0, fmt.Errorf("no gates for zone %d", zoneID)
	}
	for _, c := range candidates {
		online += c.PlayerCount
	}

	capacity = provider.ZoneCapacity(zoneID)
	if capacity == 0 {
		base := online
		if base == 0 {
			base = 1
		}
		mult := softCapMultiplier
		if mult < 1.0 {
			mult = 1.0
		}
		capacity = uint32(float64(base) * mult)
	}

	admitted, err = queue.AdmittedCount(ctx, zoneID)
	if err != nil {
		err = fmt.Errorf("admitted count: %w", err)
		return
	}

	used := online + admitted
	if capacity > used {
		free = capacity - used
	} else {
		free = 0
	}
	return
}

// ZoneCapacityFromMap is a small helper for the typical config layout
// (`map[string]uint32` keyed by zone_id). Returns 0 when missing.
func ZoneCapacityFromMap(m map[string]uint32, zoneID uint32) uint32 {
	if m == nil {
		return 0
	}
	if v, ok := m[strconv.FormatUint(uint64(zoneID), 10)]; ok {
		return v
	}
	return 0
}
