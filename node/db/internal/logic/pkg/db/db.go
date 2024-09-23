package db

import (
	"database/sql"
	"db/internal/config"
	"db/internal/logic/pkg/queue"
	"db/pb/game"
	"fmt"
	"github.com/go-sql-driver/mysql"
	"github.com/golang/protobuf/proto"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
	"log"
)

type GameDB struct {
	PBDB     *pbmysql.PbMysqlDB
	MsgQueue *queue.MsgQueue
	DB       *sql.DB
}

var DB *GameDB

func newMysqlConfig(conf config.DBConf) *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = conf.User
	myCnf.Passwd = conf.Passwd
	myCnf.Addr = conf.Addr
	myCnf.Net = conf.Net
	myCnf.DBName = conf.DBName
	return myCnf
}

// 创建数据库的函数
func CreateDatabase(conf config.DBConf) error {
	// 使用不包含数据库名的连接配置
	mysqlConfig := newMysqlConfig(conf)
	mysqlConfig.DBName = "" // 确保不指定数据库名

	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	// 创建一个与 MySQL 服务器的连接
	tempDB := sql.OpenDB(conn)
	defer func() {
		if err := tempDB.Close(); err != nil {
			logx.Error("error closing temp database connection: %w", err)
		}
	}()

	// 执行创建数据库的 SQL 语句
	_, err = tempDB.Exec(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS %s", conf.DBName))
	if err != nil {
		logx.Error("error creating database: %w", err)
		return err
	}

	return nil
}

func openDB(conf config.DBConf) error {
	// 创建数据库
	if err := CreateDatabase(conf); err != nil {
		return err
	}

	mysqlConfig := newMysqlConfig(conf)
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}

	DB = &GameDB{
		DB:       sql.OpenDB(conn),
		PBDB:     pbmysql.NewPb2DbTables(),
		MsgQueue: queue.NewMsgQueue(conf.RoutineNum, conf.ChannelBufferNum),
	}

	DB.DB.SetMaxOpenConns(conf.MaxOpenConn)
	DB.DB.SetMaxIdleConns(conf.MaxIdleConn)

	err = DB.PBDB.OpenDB(DB.DB, mysqlConfig.DBName)
	if err != nil {
		return err
	}
	return nil
}

func InitDB(conf config.DBConf) {
	if err := openDB(conf); err != nil {
		log.Fatalf("error opening database: %v", err)
	}
	createDBTable()
	alterDBTable()
	initDBConsume()
}

func createDBTable() {
	tables := []proto.Message{
		&game.UserAccounts{},
		&game.AccountShareDatabase{},
		&game.PlayerCentreDatabase{},
		&game.PlayerDatabase{},
	}

	for _, table := range tables {
		DB.PBDB.AddMysqlTable(table)
		sql := DB.PBDB.GetCreateTableSql(table)
		_, err := DB.DB.Exec(sql)
		if err != nil {
			log.Fatal(err)
			return
		}
	}
}

func alterDBTable() {
	tables := []proto.Message{
		&game.UserAccounts{},
		&game.AccountShareDatabase{},
		&game.PlayerCentreDatabase{},
		&game.PlayerDatabase{},
	}

	for _, table := range tables {
		DB.PBDB.UpdateTableField(table)
	}
}

func initDBConsume() {
	go func() {
		for i := 0; i < DB.MsgQueue.RoutineNum; i++ {
			go func(i int) {
				for {
					msg := DB.MsgQueue.Pop(i)
					DB.PBDB.LoadOneByWhereCase(msg.Body, msg.WhereCase)
					msg.Chan <- true
				}
			}(i)
		}
	}()
}
