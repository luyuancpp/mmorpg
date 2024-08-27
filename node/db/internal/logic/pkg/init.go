package pkg

import (
	"db/internal/config"
	"db/internal/logic/pkg/db"
)

func Init() {
	db.InitDB(config.DBConfig)
}
