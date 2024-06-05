package data

import cmap "github.com/orcaman/concurrent-map/v2"

func Init() {
	SessionList = cmap.New[*Player]()
}
