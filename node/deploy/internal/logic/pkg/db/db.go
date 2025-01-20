package db

import (
	"database/sql"
	"deploy/internal/config"
	"deploy/pb/game"
	"fmt"
	"github.com/go-sql-driver/mysql"
	"github.com/golang/protobuf/proto"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
)

var db *sql.DB
var PbDb *pbmysql.PbMysqlDB

func NewMysqlConfig() *mysql.Config {
	myCnf := mysql.NewConfig()
	myCnf.User = config.DeployConfig.User
	myCnf.Passwd = config.DeployConfig.Passwd
	myCnf.Addr = config.DeployConfig.Addr
	myCnf.Net = config.DeployConfig.Net
	myCnf.DBName = config.DeployConfig.DBName
	return myCnf
}

// 创建数据库的函数
func CreateDatabase() error {
	// 使用不包含数据库名的连接配置
	mysqlConfig := NewMysqlConfig()
	mysqlConfig.DBName = "" // 确保不指定数据库名

	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	// 创建一个与 MySQL 服务器的连接
	tempDB := sql.OpenDB(conn)
	defer tempDB.Close()

	// 执行创建数据库的 SQL 语句
	_, err = tempDB.Exec(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS %s", config.DeployConfig.DBName))
	if err != nil {
		logx.Error("error creating database: %w", err)
	}

	return err
}

func OpenDB() error {
	// 创建数据库
	if err := CreateDatabase(); err != nil {
		return err
	}

	// 创建带有数据库名的连接配置
	mysqlConfig := NewMysqlConfig()
	conn, err := mysql.NewConnector(mysqlConfig)
	if err != nil {
		return fmt.Errorf("error creating MySQL connector: %w", err)
	}

	db = sql.OpenDB(conn)
	db.SetMaxOpenConns(config.DeployConfig.MaxOpenConn)
	db.SetMaxIdleConns(config.DeployConfig.MaxIdleConn)

	PbDb = pbmysql.NewPb2DbTables()
	if err := PbDb.OpenDB(db, mysqlConfig.DBName); err != nil {
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
		PbDb.AddMysqlTable(table)
		sqlQuery := PbDb.GetCreateTableSql(table)
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
		PbDb.UpdateTableField(table)
	}
}

func InitDB() {
	if err := OpenDB(); err != nil {
		logx.Error("error opening database: %v", err)
		return
	}
	InitDBTable()
	AlterCreateDBTable()
}
