package cpp

import (
	"fmt"
	"github.com/iancoleman/strcase"
	"github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"log"
	"os"
	"pbgen/config"
	"pbgen/internal"
	"pbgen/utils"
	"strings"
	"text/template"

	"google.golang.org/protobuf/types/descriptorpb"
)

const playerLoaderTemplate = `
#include "threading/registry_manager.h"
#include "proto/logic/database/mysql_database_table.pb.h"

void {{.HandlerName}}MessageFieldsUnmarshal(entt::entity player, const {{.MessageType}}& message){
	{{- range .Fields }}
	{{- if .TypeName }}
	tlsRegistryManager.actorRegistry.emplace<{{.TypeName}}>(player, message.{{.Name}}());
	{{- end }}
	{{- end }}
}

void {{.HandlerName}}MessageFieldsMarshal(entt::entity player, {{.MessageType}}& message){
	{{- range .Fields }}
	{{- if .TypeName }}
	message.mutable_{{.Name}}()->CopyFrom(tlsRegistryManager.actorRegistry.get_or_emplace<{{.TypeName}}>(player));
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
	MessageType string
	Entries     []HeaderEntry
}

const playerHeaderTemplate = `#pragma once
#include "entt/src/entt/entity/registry.hpp"
#include "proto/logic/database/mysql_database_table.pb.h"
{{- range .Entries }}
void {{.HandlerName}}MessageFieldsUnmarshal(entt::entity player, const {{.MessageType}}& message);
void {{.HandlerName}}MessageFieldsMarshal(entt::entity player, {{.MessageType}}& message);
{{ end }}

void PlayerAllDataMessageFieldsMarshal(entt::entity player, PlayerAllData& message)
{
{{- range .Entries }}
{{.HandlerName}}MessageFieldsMarshal(player, *message.mutable_{{.MessageType}}_data());
{{- end }}
}

void PlayerAllDataMessageFieldsUnMarshal(entt::entity player, const PlayerAllData& message)
{
{{- range .Entries }}
{{.HandlerName}}MessageFieldsUnmarshal(player, message.{{.MessageType}}_data());
{{- end }}
}
`

type HeaderEntry struct {
	HandlerName string // 如 "PlayerDatabase1"
	MessageType string // 如 "player_database1"
}

type HeaderTemplateInput struct {
	Entries []HeaderEntry
}

func GenerateCppPlayerHeaderFile(outputPath string, entries []HeaderEntry) error {
	tmpl, err := template.New("loader").Parse(playerHeaderTemplate)
	if err != nil {
		return fmt.Errorf("template parse failed: %w", err)
	}

	f, err := os.Create(outputPath)
	if err != nil {
		return fmt.Errorf("failed to create header file: %w", err)
	}
	defer f.Close()

	data := HeaderTemplateInput{Entries: entries}
	if err := tmpl.Execute(f, data); err != nil {
		return fmt.Errorf("template execute failed: %w", err)
	}

	return nil
}

// 工具函数：判断消息是否设置了 OptionIsPlayerDatabase 为 true
func isPlayerDatabase(messageDesc *descriptorpb.DescriptorProto) bool {
	opts := messageDesc.GetOptions()
	if opts == nil {
		return false
	}

	// 读取自定义选项 OptionIsPlayerDatabase（替代原有的 OptionTableName 判断）
	extValue := proto.GetExtension(opts, messageoption.E_OptionIsPlayerDatabase)

	// 转换为 bool 类型并判断是否为 true
	isPlayerDB, ok := extValue.(bool)
	return ok && isPlayerDB
}

// 从 Descriptor Set 文件中读取消息结构（核心逻辑修改）
func CppPlayerDataLoadGenerator() {
	os.MkdirAll(config.PlayerStorageTempDirectory, os.FileMode(0777))

	var headerEntries []HeaderEntry

	// 收集所有标记为玩家数据库的消息
	for _, fileDesc := range internal.FdSet.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			// 替换判断条件：使用 isPlayerDatabase 替代 hasValidOptionTableName
			if !isPlayerDatabase(messageDesc) {
				continue
			}

			handleName := strcase.ToCamel(*messageDesc.Name)
			messageType := *messageDesc.Name
			headerEntries = append(headerEntries, HeaderEntry{
				HandlerName: handleName,
				MessageType: messageType,
			})
		}
	}

	// 为每个玩家数据库消息生成处理代码
	for _, fileDesc := range internal.FdSet.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			// 替换判断条件：仅处理标记为玩家数据库的消息
			if !isPlayerDatabase(messageDesc) {
				continue
			}

			messageDescName := strings.ToLower(*messageDesc.Name)
			handleName := strcase.ToCamel(*messageDesc.Name)
			md5FilePath := config.PlayerStorageTempDirectory + messageDescName + config.LoaderCppExtension
			filedList := generateDatabaseFiles(messageDesc)
			messageType := *messageDesc.Name

			err := generateCppDeserializeFromDatabase(
				md5FilePath,
				handleName,
				filedList,
				messageType,
				headerEntries)

			if err != nil {
				log.Fatal(err)
				return
			}

			destFilePath := config.PlayerStorageSystemDirectory + messageDescName + config.LoaderCppExtension
			err = utils.CopyFileIfChanged(md5FilePath, destFilePath)
			if err != nil {
				log.Fatal(err)
				return
			}
		}
	}

	// 生成头部文件
	md5FilePath := config.PlayerStorageTempDirectory + config.PlayerDataLoaderName
	err := GenerateCppPlayerHeaderFile(md5FilePath, headerEntries)
	if err != nil {
		log.Fatalf("failed to generate header file: %v", err)
	}

	destFilePath := config.PlayerStorageSystemDirectory + config.PlayerDataLoaderName
	err = utils.CopyFileIfChanged(md5FilePath, destFilePath)
	if err != nil {
		log.Fatalf("failed to generate header file: %v", err)
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

// 生成数据库字段列表（保持不变）
func generateDatabaseFiles(descriptor *descriptorpb.DescriptorProto) []PlayerDBProtoFieldData {
	result := make([]PlayerDBProtoFieldData, len(descriptor.GetField()))

	for i, field := range descriptor.GetField() {
		result[i].Name = field.GetName()
		result[i].TypeName = strings.ReplaceAll(field.GetTypeName(), ".", "")
	}

	return result
}

// 生成 C++ 反序列化代码（保持不变）
func generateCppDeserializeFromDatabase(fileName string, handlerName string, fields []PlayerDBProtoFieldData, messageType string, entries []HeaderEntry) error {
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
		MessageType: messageType,
		Entries:     entries,
	}

	if err := tmpl.Execute(file, data); err != nil {
		return fmt.Errorf("could not execute template: %w", err)
	}

	return nil
}
