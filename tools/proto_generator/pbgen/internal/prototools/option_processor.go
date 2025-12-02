package prototools

import (
	"errors"
	"log"
	"strings"
	"sync"

	messageoption "github.com/luyuancpp/protooption"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/runtime/protoimpl"
	"google.golang.org/protobuf/types/descriptorpb"
)

//
// ---------------- Option Type Definitions ----------------
//

type OptionType int

const (
	OptionTypeFile OptionType = iota
	OptionTypeService
	OptionTypeMethod
	OptionTypeMessage
	OptionTypeField
)

type OptionHandler func(option interface{}) error

type OptionConfig struct {
	Type     OptionType
	Handlers []OptionHandler
}

//
// ---------------- Entry: Process All Options ----------------
//

func ProcessAllOptions(wg *sync.WaitGroup, protoFiles []*descriptorpb.FileDescriptorProto) {
	configs := []OptionConfig{
		{
			Type: OptionTypeFile,
			Handlers: []OptionHandler{
				ProcessFileDefaultNode,
				ProcessFileCustomOptions,
			},
		},
		{
			Type: OptionTypeService,
			Handlers: []OptionHandler{
				ProcessServicePlayerOption,
				ProcessServiceClientOption,
				ProcessServiceCustomOptions,
			},
		},
		{
			Type: OptionTypeMethod,
			Handlers: []OptionHandler{
				ProcessMethodOperateType,
				ProcessMethodCustomOptions,
			},
		},
		{
			Type: OptionTypeMessage,
			Handlers: []OptionHandler{
				ProcessMessageTableOption,
				ProcessMessageKeyOption,
				ProcessMessageAttributeSync,
				ProcessMessageCustomOptions,
			},
		},
		{
			Type: OptionTypeField,
			Handlers: []OptionHandler{
				ProcessFieldDBOptions,
				ProcessFieldCustomOptions,
			},
		},
	}

	for _, file := range protoFiles {
		file := file
		wg.Add(1)
		go func() {
			defer wg.Done()
			processFileOptions(wg, file, configs)
		}()
	}
}

//
// ---------------- File Processing ----------------
//

func processFileOptions(wg *sync.WaitGroup, file *descriptorpb.FileDescriptorProto, configs []OptionConfig) {
	processOptionType(wg, OptionTypeFile, file.Options, configs)

	for _, svc := range file.Service {
		svc := svc
		processOptionType(wg, OptionTypeService, svc.Options, configs)

		for _, method := range svc.Method {
			method := method
			processOptionType(wg, OptionTypeMethod, method.Options, configs)
		}
	}

	for _, msg := range file.MessageType {
		msg := msg
		processOptionType(wg, OptionTypeMessage, msg.Options, configs)
		processMessageFields(wg, msg, configs)
		processNestedMessages(wg, msg, configs)
	}
}

func processMessageFields(wg *sync.WaitGroup, msg *descriptorpb.DescriptorProto, configs []OptionConfig) {
	for _, field := range msg.Field {
		field := field
		processOptionType(wg, OptionTypeField, field.Options, configs)
	}
}

func processNestedMessages(wg *sync.WaitGroup, msg *descriptorpb.DescriptorProto, configs []OptionConfig) {
	for _, n := range msg.NestedType {
		n := n
		processOptionType(wg, OptionTypeMessage, n.Options, configs)
		processMessageFields(wg, n, configs)
		processNestedMessages(wg, n, configs)
	}
}

//
// ---------------- Option Dispatch ----------------
//

func processOptionType(wg *sync.WaitGroup, optType OptionType, option interface{}, configs []OptionConfig) {
	var target OptionConfig
	for _, cfg := range configs {
		if cfg.Type == optType {
			target = cfg
			break
		}
	}

	for _, handler := range target.Handlers {
		handler := handler
		option := option

		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := handler(option); err != nil {
				log.Fatalf("Option handler failed: %v", err)
			}
		}()
	}
}

//
// ---------------- Option Handlers ----------------
//

// FileOptions: 默认节点
func ProcessFileDefaultNode(option interface{}) error {
	opts, ok := option.(*descriptorpb.FileOptions)
	if !ok {
		return errors.New("invalid FileOptions type")
	}

	val := getExt(opts, messageoption.E_OptionFileDefaultNode)
	node := val.(messageoption.NodeType)
	log.Printf("File default node: %s", messageoption.NodeType_name[int32(node)])
	return nil
}

// Service: is player service
func ProcessServicePlayerOption(option interface{}) error {
	opts, ok := option.(*descriptorpb.ServiceOptions)
	if !ok {
		return errors.New("invalid ServiceOptions type")
	}

	val := getExt(opts, messageoption.E_OptionIsPlayerService)
	log.Printf("Service.player = %v", val.(bool))
	return nil
}

// Service: client protocol
func ProcessServiceClientOption(option interface{}) error {
	opts, ok := option.(*descriptorpb.ServiceOptions)
	if !ok {
		return errors.New("invalid ServiceOptions type")
	}

	val := getExt(opts, messageoption.E_OptionIsClientProtocolService)
	log.Printf("Service.client = %v", val.(bool))
	return nil
}

// Method: operate type
func ProcessMethodOperateType(option interface{}) error {
	opts, ok := option.(*descriptorpb.MethodOptions)
	if !ok {
		return errors.New("invalid MethodOptions type")
	}

	val := getExt(opts, messageoption.E_OptionOperateType)
	t := val.(messageoption.OPERATE_TYPE)
	log.Printf("Method operate: %s", messageoption.OPERATE_TYPE_name[int32(t)])
	return nil
}

// Message: table options
func ProcessMessageTableOption(option interface{}) error {
	opts, ok := option.(*descriptorpb.MessageOptions)
	if !ok {
		return errors.New("invalid MessageOptions type")
	}

	if name, _ := getString(opts, messageoption.E_OptionTableName); name != "" {
		log.Printf("Table name: %s", name)
	}
	if id, _ := getUint(opts, messageoption.E_OptionTableId); id > 0 {
		log.Printf("Table id  : %d", id)
	}
	if backup, _ := getString(opts, messageoption.E_OptionBackupTableName); backup != "" {
		log.Printf("Backup table: %s", backup)
	}

	return nil
}

// Message: keys
func ProcessMessageKeyOption(option interface{}) error {
	opts, ok := option.(*descriptorpb.MessageOptions)
	if !ok {
		return errors.New("invalid MessageOptions type")
	}

	if pk, _ := getString(opts, messageoption.E_OptionPrimaryKey); pk != "" {
		log.Printf("Primary key: %s", pk)
	}
	if fk, _ := getString(opts, messageoption.E_OptionForeignKey); fk != "" {
		ref, _ := getString(opts, messageoption.E_OptionForeignReferences)
		log.Printf("Foreign key: %s => %s", fk, ref)
	}
	if auto, _ := getString(opts, messageoption.E_OptionAutoIncrementKey); auto != "" {
		log.Printf("Auto key: %s", auto)
	}

	return nil
}

// Message: attribute sync
func ProcessMessageAttributeSync(option interface{}) error {
	opts, ok := option.(*descriptorpb.MessageOptions)
	if !ok {
		return errors.New("invalid MessageOptions type")
	}

	if base, _ := getBool(opts, messageoption.E_OptionBaseAttributeSync); base {
		log.Printf("Attr sync: base")
	}
	if delta, _ := getBool(opts, messageoption.E_OptionDeltaAttributeSync); delta {
		log.Printf("Attr sync: delta")
	}

	return nil
}

// Field: DB options
func ProcessFieldDBOptions(option interface{}) error {
	opts, ok := option.(*descriptorpb.FieldOptions)
	if !ok {
		return errors.New("invalid FieldOptions type")
	}

	if r, _ := getBool(opts, messageoption.E_OptionDBReadOnly); r {
		log.Printf("Field readonly")
	}
	if p, _ := getString(opts, messageoption.E_OptionDBNamePrefix); p != "" {
		log.Printf("Field prefix: %s", p)
	}
	if lim, _ := getUint(opts, messageoption.E_OptionRepeatLimit); lim > 0 {
		log.Printf("Field repeat limit: %d", lim)
	}

	return nil
}

//
// ---------------- Custom Options (empty) ----------------
//

func ProcessFileCustomOptions(option interface{}) error    { return nil }
func ProcessServiceCustomOptions(option interface{}) error { return nil }
func ProcessMethodCustomOptions(option interface{}) error  { return nil }

func ProcessMessageCustomOptions(option interface{}) error {
	opts, ok := option.(*descriptorpb.MessageOptions)
	if !ok {
		return errors.New("invalid MessageOptions type")
	}

	if p, _ := getString(opts, messageoption.E_OptionProcedureName); p != "" {
		res, _ := getString(opts, messageoption.E_OptionProcedureResult)
		log.Printf("Procedure: %s => %s", p, res)
	}
	return nil
}

func ProcessFieldCustomOptions(option interface{}) error {
	opts, ok := option.(*descriptorpb.FieldOptions)
	if !ok {
		return errors.New("invalid FieldOptions type")
	}

	if old, _ := getString(opts, messageoption.E_OptionOldName); old != "" {
		log.Printf("Old name: %s", old)
	}
	return nil
}

//
// ---------------- Utils ----------------
//

// 判断是否 “extension 不存在”
func isMissingExt(err error) bool {
	if err == nil {
		return false
	}
	return strings.Contains(err.Error(), "no such extension")
}

func getExt(opts proto.Message, ext *protoimpl.ExtensionInfo) any {
	return proto.GetExtension(opts, ext)
}

func getString(opts proto.Message, ext *protoimpl.ExtensionInfo) (string, error) {
	val := getExt(opts, ext)
	s, ok := val.(string)
	if !ok {
		return "", errors.New("not string")
	}
	return s, nil
}

func getUint(opts proto.Message, ext *protoimpl.ExtensionInfo) (uint32, error) {
	val := getExt(opts, ext)
	u, ok := val.(uint32)
	if !ok {
		return 0, errors.New("not uint32")
	}
	return u, nil
}

func getBool(opts proto.Message, ext *protoimpl.ExtensionInfo) (bool, error) {
	val := getExt(opts, ext)
	b, ok := val.(bool)
	if !ok {
		return false, errors.New("not bool")
	}
	return b, nil
}
