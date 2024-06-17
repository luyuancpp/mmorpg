package pkg

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

	return nil
}

func InitDBTable() {

	PbDb.AddMysqlTable(&game.DatabaseNodeDb{})
	PbDb.AddMysqlTable(&game.LoginNodeDb{})
	PbDb.AddMysqlTable(&game.CentreNodeDb{})
	PbDb.AddMysqlTable(&game.RedisNodeDb{})
	PbDb.AddMysqlTable(&game.GateNodeDb{})
	PbDb.AddMysqlTable(&game.GameNodeDb{})

	_, err := Db.Exec(PbDb.GetCreateTableSql(&game.DatabaseNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.LoginNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.CentreNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.RedisNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.GateNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.GameNodeDb{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}

func AlterCreateDBTable() {
	PbDb.AlterTableAddField(&game.DatabaseNodeDb{})
	PbDb.AlterTableAddField(&game.LoginNodeDb{})
	PbDb.AlterTableAddField(&game.CentreNodeDb{})
	PbDb.AlterTableAddField(&game.RedisNodeDb{})
	PbDb.AlterTableAddField(&game.GateNodeDb{})
	PbDb.AlterTableAddField(&game.GameNodeDb{})
}

func InitDB(path string) {
	err := OpenDB(path)
	if err != nil {
		return
	}
	InitDBTable()
	AlterCreateDBTable()
}
