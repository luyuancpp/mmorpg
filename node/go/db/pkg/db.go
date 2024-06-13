package pkg

import (
	"database/sql"
	"db_server/internal/config"
	"db_server/pb/game"

	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"log"
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

func OpenDB() error {

	mysqlConfig := NewMysqlConfig(config.DB)
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}

	Db = sql.OpenDB(conn)
	Db.SetMaxOpenConns(config.DB.MaxOpenConn)
	Db.SetMaxIdleConns(config.DB.MaxIdleConn)

	PbDb = pbmysql.NewPb2DbTables()
	err = PbDb.OpenDB(Db, mysqlConfig.DBName)
	if err != nil {
		return err
	}

	return nil
}

func InitDB() {
	err := OpenDB()
	if err != nil {
		return
	}
	CreateDBTable()
	AlterDBTable()
}

func CreateDBTable() {

	PbDb.AddMysqlTable(&game.AccountDatabase{})
	PbDb.AddMysqlTable(&game.AccountShareDatabase{})
	PbDb.AddMysqlTable(&game.PlayerCentreDatabase{})
	PbDb.AddMysqlTable(&game.PlayerDatabase{})
	PbDb.AddMysqlTable(&game.PlayerUnimportanceDatabase{})

	_, err := Db.Exec(PbDb.GetCreateTableSql(&game.AccountDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.AccountShareDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.PlayerCentreDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.PlayerDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = Db.Exec(PbDb.GetCreateTableSql(&game.PlayerUnimportanceDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}

func AlterDBTable() {
	PbDb.AlterTableAddField(&game.AccountDatabase{})
	PbDb.AlterTableAddField(&game.AccountShareDatabase{})
	PbDb.AlterTableAddField(&game.PlayerCentreDatabase{})
	PbDb.AlterTableAddField(&game.PlayerDatabase{})
	PbDb.AlterTableAddField(&game.PlayerUnimportanceDatabase{})
}
