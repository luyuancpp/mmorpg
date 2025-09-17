package db

import (
	"database/sql"
	"db/internal/config"
	"encoding/json"
	"fmt"
	"github.com/go-sql-driver/mysql"
	pbmysql "github.com/luyuancpp/pbmysql-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"log"
	"os"
)

type GameDB struct {
	SqlGenerator *pbmysql.PbMysqlDB
	DB           *sql.DB
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
		DB:           sql.OpenDB(conn),
		SqlGenerator: pbmysql.NewPbMysqlDB(),
	}

	DB.DB.SetMaxOpenConns(config.AppConfig.ServerConfig.Database.MaxOpenConn)
	DB.DB.SetMaxIdleConns(config.AppConfig.ServerConfig.Database.MaxIdleConn)

	err = DB.SqlGenerator.OpenDB(DB.DB, mysqlConfig.DBName)
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

// 读取 JSON 文件并返回包含消息类型名称的切片
func readJSONFile(filePath string) ([]string, error) {
	// 读取文件内容
	data, err := os.ReadFile(filePath)
	if err != nil {
		return nil, err
	}

	// 定义结构体用来解析 JSON 数据
	var jsonData struct {
		Messages []string `json:"messages"`
	}

	// 解析 JSON 内容
	err = json.Unmarshal(data, &jsonData)
	if err != nil {
		return nil, err
	}

	// 返回消息类型名称切片
	return jsonData.Messages, nil
}

// 根据消息类型名称动态创建 Protobuf 消息实例
func getTablesFromJSON() ([]proto.Message, error) {
	// 读取 JSON 文件
	messageNames, err := readJSONFile(config.AppConfig.ServerConfig.JsonPath)
	if err != nil {
		return nil, fmt.Errorf("读取 JSON 文件出错: %v", err)
	}

	var messages []proto.Message

	// 遍历消息类型名称
	for _, typeName := range messageNames {
		// 动态查找 Protobuf 消息类型
		msgType, err := protoregistry.GlobalTypes.FindMessageByName(protoreflect.FullName(typeName))
		if err != nil {
			logx.Error(err)
			continue
		}

		// 使用 msgType.New() 创建新的 Protobuf 消息实例
		msg := msgType.New().Interface() // 返回 proto.Message 类型
		messages = append(messages, msg)
	}

	return messages, nil
}

func createDBTable() {
	tables, err := getTablesFromJSON() // 处理返回的错误
	if err != nil {
		log.Fatalf("error getting tables: %v", err)
		return
	}

	for _, table := range tables {
		DB.SqlGenerator.RegisterTable(table)
		sql := DB.SqlGenerator.GetCreateTableSql(table)
		_, err := DB.DB.Exec(sql)
		if err != nil {
			log.Fatal(err)
			return
		}
	}
}

// 你需要定义 getTables 函数，假设它返回一个包含表的列表
func getTables() []proto.Message {
	// 示例：返回一个空的切片，具体内容你可以根据需要调整
	return []proto.Message{}
}

func alterDBTable() {
	tables := getTables()

	for _, table := range tables {
		DB.SqlGenerator.UpdateTableField(table)
	}
}
