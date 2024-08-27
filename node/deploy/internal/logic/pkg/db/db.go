package db

import (
	"database/sql"
	"deploy/internal/config"
	"deploy/pb/game"
	"encoding/json"
	"fmt"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"log"
	"os"
)

var db *sql.DB

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

	db = sql.OpenDB(conn)
	db.SetMaxOpenConns(dbConfig.MaxOpenConn)
	db.SetMaxIdleConns(dbConfig.MaxIdleConn)

	PbDb = pbmysql.NewPb2DbTables()
	err = PbDb.OpenDB(db, mysqlConfig.DBName)
	if err != nil {
		return err
	}

	return nil
}

func InitDBTable() {

	PbDb.AddMysqlTable(&game.DatabaseNodeDb{})
	PbDb.AddMysqlTable(&game.LoginNodeDb{})
	PbDb.AddMysqlTable(&game.CentreNodeDb{})
	PbDb.AddMysqlTable(&game.RedisNodeDb{})
	PbDb.AddMysqlTable(&game.GateNodeDb{})
	PbDb.AddMysqlTable(&game.GameNodeDb{})

	_, err := db.Exec(PbDb.GetCreateTableSql(&game.DatabaseNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = db.Exec(PbDb.GetCreateTableSql(&game.LoginNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = db.Exec(PbDb.GetCreateTableSql(&game.CentreNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = db.Exec(PbDb.GetCreateTableSql(&game.RedisNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = db.Exec(PbDb.GetCreateTableSql(&game.GateNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = db.Exec(PbDb.GetCreateTableSql(&game.GameNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}

func AlterCreateDBTable() {
	PbDb.UpdateTableField(&game.DatabaseNodeDb{})
	PbDb.UpdateTableField(&game.LoginNodeDb{})
	PbDb.UpdateTableField(&game.CentreNodeDb{})
	PbDb.UpdateTableField(&game.RedisNodeDb{})
	PbDb.UpdateTableField(&game.GateNodeDb{})
	PbDb.UpdateTableField(&game.GameNodeDb{})
}

func InitDB(path string) {
	err := OpenDB(path)
	if err != nil {
		return
	}
	InitDBTable()
	AlterCreateDBTable()
}
