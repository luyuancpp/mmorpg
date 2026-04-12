package consistent

import (
	"hash/fnv"
	"sort"
	"sync"
)

// Consistent implements consistent hashing for stable key-to-partition mapping.
// Uses FNV-1a hash (fast, non-crypto), RWMutex for read-heavy concurrency,
// and pre-computed virtual nodes (~100ns per lookup).
type Consistent struct {
	ring         map[uint32]int32   // Hash ring: virtual node hash -> partition ID
	sortedHashes []uint32           // Sorted virtual node hashes for binary search
	replicaCount int                // Virtual nodes per partition (default 20)
	partitionSet map[int32]struct{} // Set of added partitions
	mu           sync.RWMutex       // RWMutex: read lock for routing, write lock for add/remove
}

// NewConsistent creates a consistent hash instance.
// replicaCount: optional virtual nodes per partition (default 20).
func NewConsistent(replicaCount ...int) *Consistent {
	defaultReplica := 20
	if len(replicaCount) > 0 && replicaCount[0] > 0 {
		defaultReplica = replicaCount[0]
	}

	return &Consistent{
		ring:         make(map[uint32]int32, defaultReplica*100),
		replicaCount: defaultReplica,
		partitionSet: make(map[int32]struct{}),
	}
}

// AddPartition adds a partition to the hash ring by creating N virtual nodes.
// Duplicate additions are no-ops. Call after Kafka actually expands partitions.
func (c *Consistent) AddPartition(partition int32) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if _, exists := c.partitionSet[partition]; exists {
		return
	}

	for i := 0; i < c.replicaCount; i++ {
		replicaKey := genReplicaKey(partition, i)
		hashVal := fnvHash(replicaKey)
		c.ring[hashVal] = partition
		c.sortedHashes = append(c.sortedHashes, hashVal)
	}

	c.partitionSet[partition] = struct{}{}
	sort.Slice(c.sortedHashes, func(i, j int) bool {
		return c.sortedHashes[i] < c.sortedHashes[j]
	})
}

// GetPartition routes a key to a partition.
// Returns the partition ID and true on success, or (0, false) if the ring is empty.
func (c *Consistent) GetPartition(key string) (int32, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if len(c.ring) == 0 {
		return 0, false
	}

	keyHash := fnvHash([]byte(key))
	idx := sort.Search(len(c.sortedHashes), func(i int) bool {
		return c.sortedHashes[i] >= keyHash
	})

	// Wrap around the ring
	if idx == len(c.sortedHashes) {
		idx = 0
	}

	return c.ring[c.sortedHashes[idx]], true
}

// GetPartitionCount returns the number of partitions in the hash ring.
func (c *Consistent) GetPartitionCount() int {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return len(c.partitionSet)
}

// GetPartitions returns a sorted copy of all partition IDs in the hash ring.
func (c *Consistent) GetPartitions() []int32 {
	c.mu.RLock()
	defer c.mu.RUnlock()

	partitions := make([]int32, 0, len(c.partitionSet))
	for p := range c.partitionSet {
		partitions = append(partitions, p)
	}
	sort.Slice(partitions, func(i, j int) bool {
		return partitions[i] < partitions[j]
	})
	return partitions
}

// genReplicaKey builds a unique key for a virtual node: partition ID (4 bytes) + replica index (4 bytes).
func genReplicaKey(partition int32, replicaIdx int) []byte {
	return []byte{
		byte(partition >> 24), byte(partition >> 16), byte(partition >> 8), byte(partition),
		byte(replicaIdx >> 24), byte(replicaIdx >> 16), byte(replicaIdx >> 8), byte(replicaIdx),
	}
}

// fnvHash computes FNV-1a hash.
func fnvHash(data []byte) uint32 {
	h := fnv.New32a()
	_, _ = h.Write(data) // fnv.Write never returns an error
	return h.Sum32()
}
