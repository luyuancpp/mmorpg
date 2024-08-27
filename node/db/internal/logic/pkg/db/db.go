package db

import (
	"database/sql"
	"db/internal/config"
	"db/internal/logic/pkg/queue"
	"db/pb/game"
	"fmt"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
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

// 创建数据库的函数
func CreateDatabase(config config.DBConf) error {
	// 使用不包含数据库名的连接配置
	mysqlConfig := newMysqlConfig(config)
	mysqlConfig.DBName = "" // 确保不指定数据库名

	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	// 创建一个与 MySQL 服务器的连接
	tempDB := sql.OpenDB(conn)
	defer tempDB.Close()

	// 执行创建数据库的 SQL 语句
	_, err = tempDB.Exec(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS %s", config.DBName))
	if err != nil {
		logx.Error("error creating database: %w", err)
	}

	return err
}

func openDB() error {
	// 创建数据库
	if err := CreateDatabase(config.DBConfig); err != nil {
		return err
	}

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
	DB.PbDB.AddMysqlTable(&game.UserAccounts{})
	DB.PbDB.AddMysqlTable(&game.AccountShareDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerCentreDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerDatabase{})
	DB.PbDB.AddMysqlTable(&game.PlayerUnimportanceDatabase{})

	_, err := DB.DB.Exec(DB.PbDB.GetCreateTableSql(&game.UserAccounts{}))
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
	DB.PbDB.UpdateTableField(&game.UserAccounts{})
	DB.PbDB.UpdateTableField(&game.AccountShareDatabase{})
	DB.PbDB.UpdateTableField(&game.PlayerCentreDatabase{})
	DB.PbDB.UpdateTableField(&game.PlayerDatabase{})
	DB.PbDB.UpdateTableField(&game.PlayerUnimportanceDatabase{})
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
