package pkg

import (
	"database/sql"
	"deploy_server/internal/config"
	"deploy_server/pb/deploy"
	"encoding/json"
	"fmt"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"log"
	"os"
)

var Db *sql.DB

var PbDb *pbmysql.PbMysqlDB

func NewMysqlConfig(config config.DBConfig) *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = config.User
	myCnf.Passwd = config.Passwd
	myCnf.Addr = config.Addr
	myCnf.Net = config.Net
	myCnf.DBName = config.DBName
	return myCnf
}

func OpenDB(path string) error {
	file, err := os.Open(path)
	defer func(file *os.File) {
		err := file.Close()
		if err != nil {
			fmt.Println(err)
		}
	}(file)
	if err != nil {
		fmt.Println(err)
		return err
	}

	decoder := json.NewDecoder(file)
	dbConfig := config.DBConfig{}
	err = decoder.Decode(&dbConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}
	mysqlConfig := NewMysqlConfig(dbConfig)
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}

	Db = sql.OpenDB(conn)
	Db.SetMaxOpenConns(dbConfig.MaxOpenConn)
	Db.SetMaxIdleConns(dbConfig.MaxIdleConn)

	PbDb = pbmysql.NewPb2DbTables()
	PbDb.AddMysqlTable(&deploy.DatabaseServerDb{})
	PbDb.AddMysqlTable(&deploy.LoginServerDb{})
	PbDb.AddMysqlTable(&deploy.ControllerServerDb{})
	PbDb.AddMysqlTable(&deploy.RedisServerDb{})
	PbDb.AddMysqlTable(&deploy.GateServerDb{})

	return nil
}

func InitDBTables() {
	_, err := Db.Exec(PbDb.GetCreateTableSql(&deploy.DatabaseServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&deploy.LoginServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&deploy.ControllerServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&deploy.RedisServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&deploy.GateServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}
