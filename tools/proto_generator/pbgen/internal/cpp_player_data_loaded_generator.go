package internal

import (
	"fmt"
	"github.com/iancoleman/strcase"
	"log"
	"os"
	"pbgen/internal/config"
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
	message.mutable_{{.Name}}()->CopyFrom(tlsRegistryManager.actorRegistry.get<{{.TypeName}}>(player));
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
{{.HandlerName}}MessageFieldsUnmarshal(player, message.{{.MessageType}}_data());
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
	tmpl, err := template.New("player_header").Parse(playerHeaderTemplate)
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

// 从 Descriptor Set 文件中读取消息结构
func CppPlayerDataLoadGenerator() {
	os.MkdirAll(config.PlayerStorageTempDirectory, os.FileMode(0777))

	var headerEntries []HeaderEntry

	for _, fileDesc := range FdSet.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			messageDescName := strings.ToLower(*messageDesc.Name)
			if !(strings.Contains(messageDescName, config.PlayerDatabaseName) || strings.Contains(messageDescName, config.PlayerDatabaseName1)) {
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

	// 遍历文件描述符集合并打印消息字段
	for _, fileDesc := range FdSet.GetFile() {
		for _, messageDesc := range fileDesc.GetMessageType() {
			messageDescName := strings.ToLower(*messageDesc.Name)
			if !(strings.Contains(messageDescName, config.PlayerDatabaseName) || strings.Contains(messageDescName, config.PlayerDatabaseName1)) {
				continue
			}

			//printMessageFields(messageDesc)

			handleName := strcase.ToCamel(*messageDesc.Name)
			md5FilePath := config.PlayerStorageTempDirectory + "player_" + messageDescName
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

			destFilePath := config.PlayerStorageSystemDirectory + "player_" + messageDescName

			err = CopyFileIfChanged(md5FilePath, destFilePath)
			if err != nil {
				log.Fatal(err)
				return
			}

		}
	}

	err := GenerateCppPlayerHeaderFile(config.PlayerStorageSystemDirectory+config.PlayerDataLoaderName, headerEntries)
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
