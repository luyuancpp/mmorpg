package cpp

import (
	"fmt"
	"os"
	"strings"
	"sync"

	"github.com/iancoleman/strcase"
	messageoption "github.com/luyuancpp/protooption"
	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"
)

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

type HeaderEntry struct {
	HandlerName string // e.g. "PlayerDatabase1"
	MessageType string // e.g. "player_database1"
}

type HeaderTemplateInput struct {
	Entries []HeaderEntry
}

func GenerateCppPlayerHeaderFile(outputPath string, entries []HeaderEntry) error {
	data := HeaderTemplateInput{Entries: entries}
	return utils.RenderTemplateToFile("internal/template/player_data_loader.h.tmpl", outputPath, data)
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
			logger.Global.Error("Failed to generate player data loader: directory creation failed",
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
					logger.Global.Fatal("Failed to generate player data deserialization code",
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
			logger.Global.Fatal("Failed to generate player data loader header file",
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
	data := DescData{
		Fields:      fields,
		HandlerName: handlerName,
		MessageType: messageType,
		Entries:     entries,
	}

	return utils.RenderTemplateToFile("internal/template/player_data_loader.cpp.tmpl", fileName, data)
}

