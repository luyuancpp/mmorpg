package cpp

import (
	"bytes"
	"fmt"
	"os"
	"strings"
	"sync"
	"text/template"

	"github.com/iancoleman/strcase"
	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"

	"pbgen/internal"
	_config "pbgen/internal/config"
	"pbgen/internal/utils"
	"pbgen/logger"
)

const playerLoaderTemplate = `
#include "thread_context/registry_manager.h"
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
#include "proto/common/database/mysql_database_table.pb.h"
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
		logger.Global.Fatal("生成玩家头文件失败: 解析模板失败",
			zap.String("template_name", "loader"),
			zap.String("output_path", outputPath),
			zap.Error(err),
		)
	}

	data := HeaderTemplateInput{Entries: entries}
	var rendered bytes.Buffer
	if err := tmpl.Execute(&rendered, data); err != nil {
		logger.Global.Fatal("生成玩家头文件失败: 执行模板失败",
			zap.String("file_path", outputPath),
			zap.Error(err),
		)
	}

	return utils.WriteFileIfChanged(outputPath, []byte(utils.NormalizeGeneratedLayout(rendered.String())))
}

// isPlayerDatabase returns true if the message has OptionIsPlayerDatabase set.
func isPlayerDatabase(messageDesc *descriptorpb.DescriptorProto) bool {
	opts := messageDesc.GetOptions()
	if opts == nil {
		return false
	}

	extValue := proto.GetExtension(opts, messageoption.E_OptionIsPlayerDatabase)

	isPlayerDB, ok := extValue.(bool)
	return ok && isPlayerDB
}

// CppPlayerDataLoadGenerator generates C++ player data loader from descriptor set.
func CppPlayerDataLoadGenerator(wg *sync.WaitGroup) {
	wg.Add(1)

	go func() {
		defer wg.Done()

		err := os.MkdirAll(_config.Global.Paths.PlayerStorageTempDir, os.FileMode(0777))
		if err != nil {
			logger.Global.Error("生成玩家数据加载器失败: 创建临时目录失败",
				zap.String("directory", _config.Global.Paths.PlayerStorageTempDir),
				zap.Error(err),
			)
			return
		}

		var headerEntries []HeaderEntry

		for _, fileDesc := range internal.FdSet.GetFile() {
			for _, messageDesc := range fileDesc.GetMessageType() {
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

		for _, fileDesc := range internal.FdSet.GetFile() {
			for _, messageDesc := range fileDesc.GetMessageType() {
				if !isPlayerDatabase(messageDesc) {
					continue
				}

				messageDescName := strings.ToLower(*messageDesc.Name)
				handleName := strcase.ToCamel(*messageDesc.Name)
				filePath := _config.Global.Paths.PlayerStorageTempDir + messageDescName + _config.Global.FileExtensions.LoaderCpp
				filedList := generateDatabaseFiles(messageDesc)
				messageType := *messageDesc.Name

				err := generateCppDeserializeFromDatabase(
					filePath,
					handleName,
					filedList,
					messageType,
					headerEntries)

				if err != nil {
					logger.Global.Fatal("生成玩家数据反序列化代码失败",
						zap.String("message_type", messageType),
						zap.String("file_path", filePath),
						zap.Error(err),
					)
					return
				}
			}
		}

		err = GenerateCppPlayerHeaderFile(_config.Global.Paths.PlayerDataLoaderFile, headerEntries)
		if err != nil {
			logger.Global.Fatal("生成玩家数据加载器头文件失败",
				zap.String("file_path", _config.Global.Paths.PlayerDataLoaderFile),
				zap.Error(err),
			)
		}
	}()
}

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

func generateDatabaseFiles(descriptor *descriptorpb.DescriptorProto) []PlayerDBProtoFieldData {
	result := make([]PlayerDBProtoFieldData, len(descriptor.GetField()))

	for i, field := range descriptor.GetField() {
		result[i].Name = field.GetName()
		result[i].TypeName = strings.ReplaceAll(field.GetTypeName(), ".", "")
	}

	return result
}

func generateCppDeserializeFromDatabase(fileName string, handlerName string, fields []PlayerDBProtoFieldData, messageType string, entries []HeaderEntry) error {
	tmpl, err := template.New("handler").Parse(playerLoaderTemplate)
	if err != nil {
		logger.Global.Fatal("生成反序列化代码失败: 解析模板失败",
			zap.String("template_name", "handler"),
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}

	data := DescData{
		Fields:      fields,
		HandlerName: handlerName,
		MessageType: messageType,
		Entries:     entries,
	}

	var rendered bytes.Buffer
	if err := tmpl.Execute(&rendered, data); err != nil {
		logger.Global.Fatal("生成反序列化代码失败: 执行模板失败",
			zap.String("file_name", fileName),
			zap.String("message_type", messageType),
			zap.Error(err),
		)
	}

	return utils.WriteFileIfChanged(fileName, []byte(utils.NormalizeGeneratedLayout(rendered.String())))
}

