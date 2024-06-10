package pkg

import (
	"database/sql"
	"deploy_server/internal/config"
	"deploy_server/pb/game"
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
	err = PbDb.OpenDB(Db, mysqlConfig.DBName)
	if err != nil {
		return err
	}
	PbDb.AddMysqlTable(&game.DatabaseServerDb{})
	PbDb.AddMysqlTable(&game.LoginServerDb{})
	PbDb.AddMysqlTable(&game.CentreServerDb{})
	PbDb.AddMysqlTable(&game.RedisServerDb{})
	PbDb.AddMysqlTable(&game.GateServerDb{})

	return nil
}

func InitDBTables() {
	_, err := Db.Exec(PbDb.GetCreateTableSql(&game.DatabaseServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.LoginServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.CentreServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.RedisServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.GateServerDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}
