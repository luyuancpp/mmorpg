package data

import cmap "github.com/orcaman/concurrent-map/v2"

var SessionList cmap.ConcurrentMap[string, *Session]

func init() {
	SessionList = cmap.New[*Session]()
}
