package internal

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

const playerLoaderTemplate = `
#include "thread_local/storage.h"
#include "proto/db/mysql_database_table.pb.h"

void {{.HandlerName}}MessageFieldsUnmarshal(entt::entity player, const player_database& message){
	{{- range .Fields }}
	{{- if .TypeName }}
	tls.registry.emplace<{{.TypeName}}>(player, message.{{.Name}}());
	{{- end }}
	{{- end }}
}

void {{.HandlerName}}MessageFieldsMarshal(entt::entity player, player_database& message){
	{{- range .Fields }}
	{{- if .TypeName }}
	message.mutable_{{.Name}}()->CopyFrom(tls.registry.get<{{.TypeName}}>(player));
	{{- end }}
	{{- end }}
}
`

type PlayerDBProtoFieldData struct {
	Name     string
	TypeName string
}

type DescData struct {
	Fields      []PlayerDBProtoFieldData
	HandlerName string
}

// 从 Descriptor Set 文件中读取消息结构
func CppPlayerDataLoadGenerator() {
	// 生成文件路径
	descFilePath := filepath.Join(
		config.PbDescDirectory,
		config.GameMysqlDBProtoFileName+config.ProtoDescExtension,
	)

	os.MkdirAll(config.PlayerStorageTempDirectory, os.FileMode(0777))

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

			//printMessageFields(messageDesc)

			handleName := util.CapitalizeWords(*messageDesc.Name)

			md5FilePath := config.PlayerStorageTempDirectory + "player_" + messageDescName + config.CppSystemExtension

			filedList := generateDatabaseFiles(messageDesc)

			err := generateCppDeserializeFromDatabase(
				md5FilePath,
				handleName,
				filedList)

			if err != nil {
				log.Fatal(err)
				return
			}

			destFilePath := config.PlayerStorageSystemDirectory + "player_" + messageDescName + config.CppSystemExtension

			err = CopyFileIfChanged(md5FilePath, destFilePath)
			if err != nil {
				log.Fatal(err)
				return
			}

		}
	}
}

// 打印消息字段信息
func printMessageFields(descriptor *descriptorpb.DescriptorProto) {
	fmt.Printf("Message Type: %s\n", descriptor.GetName())
	for _, field := range descriptor.GetField() {
		fieldName := field.GetName()
		fieldType := field.GetType()
		fieldLabel := field.GetLabel()
		fieldTypeName := field.GetTypeName()

		fmt.Printf("Field %s: %s (Type: %s, Label: %s)\n", fieldTypeName, fieldName, fieldType, fieldLabel)
	}
}

// generateHandlerCases creates the cases for the switch statement based on the method.
func generateDatabaseFiles(descriptor *descriptorpb.DescriptorProto) []PlayerDBProtoFieldData {
	result := make([]PlayerDBProtoFieldData, len(descriptor.GetField()))

	for i, field := range descriptor.GetField() {
		result[i].Name = field.GetName()
		result[i].TypeName = strings.ReplaceAll(field.GetTypeName(), ".", "")
	}

	return result
}

// generateHandlerFile creates a new handler file with the specified parameters.
func generateCppDeserializeFromDatabase(fileName string, handlerName string, fields []PlayerDBProtoFieldData) error {
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
