package config

type DBConfig struct {
	Net         string `json:"Net"`
	Addr        string `json:"Addr"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
}
