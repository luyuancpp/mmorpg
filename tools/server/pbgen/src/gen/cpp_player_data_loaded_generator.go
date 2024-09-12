package gen

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"pbgen/util"
	"strings"
	"text/template"

	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"pbgen/config"
)

const playerLoaderTemplate = `package handler

#include "thread_local/storage.h"
#include "proto/common/mysql_database_table.pb.h"

void Player{{.HandlerName}}Unmarshal(entt::entity player, const player_database& message){
	{{- range .Fields }}
	tls.registry.emplace<{{.Type}}>(player, message.{{.Name}});
	{{- end }}
}

void Player{{.HandlerName}}Marshal(entt::entity player, const player_database& message){
	{{- range .Fields }}
	tls.registry.emplace<{{.Type}}>(player, message.{{.Name}});
	{{- end }}
}
`

type DescData struct {
	Fields      []*descriptorpb.FieldDescriptorProto
	HandlerName string
}

// 从 Descriptor Set 文件中读取消息结构
func CppPlayerDataLoadGenerator() {
	// 生成文件路径
	descFilePath := filepath.Join(
		config.DBDescDirectory,
		config.GameMysqlDBProtoFileName+config.ProtoDescExtension,
	)

	// 读取 Descriptor Set 文件
	data, err := os.ReadFile(descFilePath)
	if err != nil {
		log.Fatalf("Failed to read descriptor set file: %v", err)
	}

	// 解析 Descriptor Set 文件内容
	fdSet := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(data, fdSet); err != nil {
		log.Fatalf("Failed to unmarshal descriptor set: %v", err)
	}

	// 遍历文件描述符集合并打印消息字段
	for _, fileDesc := range fdSet.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			messageDescName := strings.ToLower(*messageDesc.Name)
			if !(strings.Contains(messageDescName, config.PlayerDatabaseName) || strings.Contains(messageDescName, config.PlayerDatabaseName1)) {
				continue
			}

			md5FilePath := config.PlayerStorageMd5Directory + "player_" + messageDescName + config.CppUtilExtension

			err := generateCppDeserializeFromDatabase(
				md5FilePath,
				messageDescName,
				messageDesc.GetField())

			if err != nil {
				log.Fatal(err)
				return
			}

			destFilePath := config.PlayerStorageUtilDirectory + "player_" + messageDescName + config.CppUtilExtension

			_, err = util.CopyFileByMd5(destFilePath, md5FilePath)
			if err != nil {
				log.Fatal(err)
				return
			}

		}
	}
}

// generateHandlerFile creates a new handler file with the specified parameters.
func generateCppDeserializeFromDatabase(fileName string, handlerName string, fields []*descriptorpb.FieldDescriptorProto) error {
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file %s: %w", fileName, err)
	}
	defer func(file *os.File) {
		err := file.Close()
		if err != nil {
			log.Fatal(err)
			return
		}
	}(file)

	tmpl, err := template.New("handler").Parse(playerLoaderTemplate)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	data := DescData{
		Fields:      fields,
		HandlerName: handlerName,
	}

	if err := tmpl.Execute(file, data); err != nil {
		return fmt.Errorf("could not execute template: %w", err)
	}

	return nil
}
