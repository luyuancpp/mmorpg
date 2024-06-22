package db

import (
	"database/sql"
	"db/internal/config"
	"db/internal/logic/pkg/queue"
	"db/pb/game"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"log"
)

type GameDB struct {
	PbDB     *pbmysql.PbMysqlDB
	MsgQueue *queue.MsgQueue
	DB       *sql.DB
}

var DB *GameDB

func newMysqlConfig(config config.DBConf) *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = config.User
	myCnf.Passwd = config.Passwd
	myCnf.Addr = config.Addr
	myCnf.Net = config.Net
	myCnf.DBName = config.DBName
	return myCnf
}

func openDB() error {
	mysqlConfig := newMysqlConfig(config.DBConfig)
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}

	DB = &GameDB{
		DB:       sql.OpenDB(conn),
		PbDB:     pbmysql.NewPb2DbTables(),
		MsgQueue: queue.NewMsgQueue(config.DBConfig.RoutineNum, config.DBConfig.ChannelBufferNum),
	}

	DB.DB.SetMaxOpenConns(config.DBConfig.MaxOpenConn)
	DB.DB.SetMaxIdleConns(config.DBConfig.MaxIdleConn)

	err = DB.PbDB.OpenDB(DB.DB, mysqlConfig.DBName)
	if err != nil {
		return err
	}
	return nil
}

func InitDB() {
	err := openDB()
	if err != nil {
		return
	}
	createDBTable()
	alterDBTable()
	initDBConsume()
}

func createDBTable() {
	DB.PbDB.AddMysqlTable(&game.AccountDatabase{})
	DB.PbDB.AddMysqlTable(&game.AccountShareDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerCentreDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerUnimportanceDatabase{})

	_, err := DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.AccountDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.AccountShareDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.PlayerCentreDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.PlayerDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.PlayerUnimportanceDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}

func alterDBTable() {
	DB.PbDB.AlterTableAddField(&game.AccountDatabase{})
	DB.PbDB.AlterTableAddField(&game.AccountShareDatabase{})
	DB.PbDB.AlterTableAddField(&game.PlayerCentreDatabase{})
	DB.PbDB.AlterTableAddField(&game.PlayerDatabase{})
	DB.PbDB.AlterTableAddField(&game.PlayerUnimportanceDatabase{})
}

func initDBConsume() {
	go func() {
		for i := 0; i < config.DBConfig.RoutineNum; i++ {
			go func(i int) {
				for {
					msg := DB.MsgQueue.Pop(i)
					DB.PbDB.LoadOneByWhereCase(msg.Body, msg.WhereCase)
					msg.Chan <- true
				}
			}(i)
		}
	}()
}
