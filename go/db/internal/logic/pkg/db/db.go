package db

import (
	"database/sql"
	"fmt"
	"game/db/internal/config"
	"game/generated/pb/game"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"log"
)

type GameDB struct {
	PBDB *pbmysql.PbMysqlDB
	DB   *sql.DB
}

var DB *GameDB

func newMysqlConfig() *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = config.AppConfig.ServerConfig.Database.User
	myCnf.Passwd = config.AppConfig.ServerConfig.Database.Passwd
	myCnf.Addr = config.AppConfig.ServerConfig.Database.Hosts
	myCnf.Net = config.AppConfig.ServerConfig.Database.Net
	myCnf.DBName = config.AppConfig.ServerConfig.Database.DBName
	return myCnf
}

// 创建数据库的函数
func CreateDatabase() error {
	// 使用不包含数据库名的连接配置
	mysqlConfig := newMysqlConfig()
	mysqlConfig.DBName = "" // 确保不指定数据库名

	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	// 创建一个与 MySQL 服务器的连接
	tempDB := sql.OpenDB(conn)
	defer func() {
		if err := tempDB.Close(); err != nil {
			logx.Errorf("error closing temp database connection: %v", err)
		}
	}()

	// 执行创建数据库的 SQL 语句
	_, err = tempDB.Exec(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS %s", config.AppConfig.ServerConfig.Database.DBName))
	if err != nil {
		logx.Error("error creating database: %w", err)
		return err
	}

	return nil
}

func openDB() error {
	// 创建数据库
	if err := CreateDatabase(); err != nil {
		return err
	}

	mysqlConfig := newMysqlConfig()
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		log.Fatal(err)
		return err
	}

	DB = &GameDB{
		DB:   sql.OpenDB(conn),
		PBDB: pbmysql.NewPbMysqlDB(),
	}

	DB.DB.SetMaxOpenConns(config.AppConfig.ServerConfig.Database.MaxOpenConn)
	DB.DB.SetMaxIdleConns(config.AppConfig.ServerConfig.Database.MaxIdleConn)

	err = DB.PBDB.OpenDB(DB.DB, mysqlConfig.DBName)
	if err != nil {
		return err
	}
	return nil
}

func InitDB() {
	if err := openDB(); err != nil {
		log.Fatalf("error opening database: %v", err)
	}
	createDBTable()
	alterDBTable()
}

// 返回一个包含多个 Protobuf 消息实例的切片
func getTables() []proto.Message {
	// 创建并返回 Protobuf 消息的切片
	return []proto.Message{
		&game.UserAccounts{},
		&game.AccountShareDatabase{},
		&game.PlayerCentreDatabase{},
		&game.PlayerDatabase{},
		&game.PlayerDatabase_1{},
	}
}

func createDBTable() {
	tables := getTables()

	for _, table := range tables {
		DB.PBDB.RegisterTable(table)
		sql := DB.PBDB.GetCreateTableSql(table)
		_, err := DB.DB.Exec(sql)
		if err != nil {
			log.Fatal(err)
			return
		}
	}
}

func alterDBTable() {
	tables := getTables()

	for _, table := range tables {
		DB.PBDB.UpdateTableField(table)
	}
}
