package pkg

import (
	"database/sql"
	"db/internal/config"
	"db/pb/game"
	"db/queue"

	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"log"
)

type GameDB struct {
	PbDB     *pbmysql.PbMysqlDB
	MsgQueue *queue.MsgQueue
	DB       *sql.DB
}

var NodeDB *GameDB

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

	NodeDB = &GameDB{
		DB:       sql.OpenDB(conn),
		PbDB:     pbmysql.NewPb2DbTables(),
		MsgQueue: queue.NewMsgQueue(config.DBConfig.RoutineNum, config.DBConfig.ChannelBufferNum),
	}

	NodeDB.DB.SetMaxOpenConns(config.DBConfig.MaxOpenConn)
	NodeDB.DB.SetMaxIdleConns(config.DBConfig.MaxIdleConn)

	err = NodeDB.PbDB.OpenDB(NodeDB.DB, mysqlConfig.DBName)
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

	NodeDB.PbDB.AddMysqlTable(&game.AccountDatabase{})
	NodeDB.PbDB.AddMysqlTable(&game.AccountShareDatabase{})
	NodeDB.PbDB.AddMysqlTable(&game.PlayerCentreDatabase{})
	NodeDB.PbDB.AddMysqlTable(&game.PlayerDatabase{})
	NodeDB.PbDB.AddMysqlTable(&game.PlayerUnimportanceDatabase{})

	_, err := NodeDB.DB.Exec(NodeDB.PbDB.GetCreateTableSql(&game.AccountDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = NodeDB.DB.Exec(NodeDB.PbDB.GetCreateTableSql(&game.AccountShareDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = NodeDB.DB.Exec(NodeDB.PbDB.GetCreateTableSql(&game.PlayerCentreDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = NodeDB.DB.Exec(NodeDB.PbDB.GetCreateTableSql(&game.PlayerDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
	_, err = NodeDB.DB.Exec(NodeDB.PbDB.GetCreateTableSql(&game.PlayerUnimportanceDatabase{}))
	if err != nil {
		log.Fatal(err)
		return
	}
}

func alterDBTable() {
	NodeDB.PbDB.AlterTableAddField(&game.AccountDatabase{})
	NodeDB.PbDB.AlterTableAddField(&game.AccountShareDatabase{})
	NodeDB.PbDB.AlterTableAddField(&game.PlayerCentreDatabase{})
	NodeDB.PbDB.AlterTableAddField(&game.PlayerDatabase{})
	NodeDB.PbDB.AlterTableAddField(&game.PlayerUnimportanceDatabase{})
}

func initDBConsume() {
	go func() {
		for i := 0; i < config.DBConfig.RoutineNum; i++ {
			go func(i int) {
				for {
					msg := NodeDB.MsgQueue.Pop(i)
					NodeDB.PbDB.LoadOneByWhereCase(msg.Body, msg.WhereCase)
					msg.Chan <- true
				}
			}(i)
		}
	}()
}
