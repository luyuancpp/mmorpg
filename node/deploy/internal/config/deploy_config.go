package config

import (
	"encoding/json"
	"github.com/zeromicro/go-zero/core/logx"
	"os"
)

type DeployConfigStruct struct {
	Net             string `json:"Net"`
	Addr            string `json:"Addr"`
	User            string `json:"User"`
	Passwd          string `json:"Passwd"`
	DBName          string `json:"DBName"`
	MaxOpenConn     int    `json:"MaxOpenConn"`
	MaxIdleConn     int    `json:"MaxIdleConn"`
	MaintenanceMode bool   `json:"MaintenanceMode"`
}

var DeployConfig DeployConfigStruct

func init() {
	file, err := os.Open("etc/deploy_config.json")
	if err != nil {
		logx.Error("error opening config file: %w", err)
		return
	}

	defer func(file *os.File) {
		err := file.Close()
		if err != nil {
			logx.Error("error opening config file: %w", err)
		}
	}(file)

	decoder := json.NewDecoder(file)

	if err := decoder.Decode(&DeployConfig); err != nil {
		logx.Error("error decoding config file: %w", err)
	}
}
