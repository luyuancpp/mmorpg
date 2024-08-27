package db

import (
	"database/sql"
	"deploy/internal/config"
	"deploy/pb/game"
	"encoding/json"
	"fmt"
	"github.com/go-sql-driver/mysql"
	"github.com/golang/protobuf/proto"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
	"os"
)

var db *sql.DB
var PBDB *pbmysql.PbMysqlDB

func NewMysqlConfig(config config.DBConfig) *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = config.User
	myCnf.Passwd = config.Passwd
	myCnf.Addr = config.Addr
	myCnf.Net = config.Net
	myCnf.DBName = config.DBName
	return myCnf
}

// 创建数据库的函数
func CreateDatabase(config config.DBConfig) error {
	// 使用不包含数据库名的连接配置
	mysqlConfig := NewMysqlConfig(config)
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

func OpenDB(path string) error {
	file, err := os.Open(path)
	if err != nil {
		logx.Error("error opening config file: %w", err)
		return err
	}
	defer file.Close()

	decoder := json.NewDecoder(file)
	dbConfig := config.DBConfig{}
	if err := decoder.Decode(&dbConfig); err != nil {
		return fmt.Errorf("error decoding config file: %w", err)
	}

	// 创建数据库
	if err := CreateDatabase(dbConfig); err != nil {
		return err
	}

	// 创建带有数据库名的连接配置
	mysqlConfig := NewMysqlConfig(dbConfig)
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	db = sql.OpenDB(conn)
	db.SetMaxOpenConns(dbConfig.MaxOpenConn)
	db.SetMaxIdleConns(dbConfig.MaxIdleConn)

	PBDB = pbmysql.NewPb2DbTables()
	if err := PBDB.OpenDB(db, mysqlConfig.DBName); err != nil {
		logx.Error("error opening PbMysqlDB: %w", err)
		return err
	}

	return nil
}

func InitDBTable() {
	tables := []proto.Message{
		&game.DatabaseNodeDb{},
		&game.LoginNodeDb{},
		&game.CentreNodeDb{},
		&game.RedisNodeDb{},
		&game.GateNodeDb{},
		&game.GameNodeDb{},
	}

	for _, table := range tables {
		PBDB.AddMysqlTable(table)
		sqlQuery := PBDB.GetCreateTableSql(table)
		_, err := db.Exec(sqlQuery)
		if err != nil {
			logx.Error("error creating table: %v", err)
		}
	}
}

func AlterCreateDBTable() {
	tables := []proto.Message{
		&game.DatabaseNodeDb{},
		&game.LoginNodeDb{},
		&game.CentreNodeDb{},
		&game.RedisNodeDb{},
		&game.GateNodeDb{},
		&game.GameNodeDb{},
	}

	for _, table := range tables {
		PBDB.UpdateTableField(table)
	}
}

func InitDB(path string) {
	if err := OpenDB(path); err != nil {
		logx.Error("error opening database: %v", err)
	}
	InitDBTable()
	AlterCreateDBTable()
}
