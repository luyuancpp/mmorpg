package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"github.com/iancoleman/strcase"
	"log"
	"math"
	"os"
	"path"
	"path/filepath"
	"pbgen/internal/config"
	"pbgen/utils"
	"sort"
	"strconv"
	"strings"
	"sync/atomic"
	"text/template"
)

// ReadProtoFileService reads service information from a protobuf descriptor file.
func ReadProtoFileService() error {
	serviceIndex := uint32(0)

	// Iterate through each file in the descriptor set
	for _, file := range FdSet.File {
		for _, service := range file.Service {
			// Create an RPCServiceInfo object for each service
			rpcServiceInfo := RPCServiceInfo{
				Fd:                     file,
				ServiceIndex:           serviceIndex,
				ServiceDescriptorProto: service,
			}

			// Iterate through each method in the service
			for index, method := range service.Method {
				// Create an MethodInfo object for each method
				rpcMethodInfo := MethodInfo{
					Id:                     math.MaxUint64,
					Index:                  uint64(index),
					Fd:                     file,
					ServiceDescriptorProto: service,
					MethodDescriptorProto:  method,
				}

				// Append the method info to the service info
				rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)

				// Increment the global message ID counter
				atomic.AddUint64(&MaxMessageId, 1)
			}

			GlobalRPCServiceList = append(GlobalRPCServiceList, &rpcServiceInfo)

			result, ok := FileServiceMap.Load(file.GetName())
			if ok {
				serviceList := result.([]*RPCServiceInfo)
				serviceList = append(serviceList, &rpcServiceInfo)
				FileServiceMap.Store(file.GetName(), serviceList)
			} else {
				var serviceList []*RPCServiceInfo
				serviceList = append(serviceList, &rpcServiceInfo)
				FileServiceMap.Store(file.GetName(), serviceList)
			}
			serviceIndex++
		}
	}

	// Return nil on successful completion
	return nil
}

// ReadAllProtoFileServices reads all service information from protobuf files in configured directories.
func ReadAllProtoFileServices() {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()
		_ = ReadProtoFileService()
	}()
}

// ReadServiceIdFile reads service IDs from a file asynchronously.
func ReadServiceIdFile() {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()

		f, err := os.Open(config.ServiceIdFilePath)
		if err != nil {
			fmt.Errorf("failed to open file %s: %w", config.ServiceIdFilePath, err)
			log.Fatalf("error reading service ID file: %v", err)
		}
		defer f.Close()

		scanner := bufio.NewScanner(f)
		for scanner.Scan() {
			line := scanner.Text()
			splitList := strings.Split(line, "=")
			id, err := strconv.ParseUint(splitList[0], 10, 64)
			if err != nil {
				fmt.Errorf("failed to parse ID from line %s: %w", line, err)
				log.Fatalf("error reading service ID file: %v", err)
			}
			ServiceIdMap[splitList[1]] = id
		}

		if err := scanner.Err(); err != nil {
			fmt.Errorf("error reading file %s: %w", config.ServiceIdFilePath, err)
			log.Fatalf("error reading service ID file: %v", err)
		}

	}()
}

func WriteServiceIdFile() {
	utils.Wg.Add(1)
	go func() {
		defer utils.Wg.Done()

		var data string
		var idList []uint64
		for k, _ := range RpcIdMethodMap {
			idList = append(idList, k)
		}
		sort.Slice(idList, func(i, j int) bool { return idList[i] < idList[j] })
		for i := 0; i < len(idList); i++ {
			rpcMethodInfo, ok := RpcIdMethodMap[idList[i]]
			if !ok {
				fmt.Println("msg id=", strconv.Itoa(i), " not use ")
				continue
			}
			data += strconv.FormatUint(rpcMethodInfo.Id, 10) + "=" + (*rpcMethodInfo).KeyName() + "\n"
		}
		WriteFileIfChanged(config.ServiceIdFilePath, []byte(data))
	}()
}

// InitServiceId initializes service IDs based on the loaded service methods and ID mappings.
func InitServiceId() {
	var unUseServiceId = make(map[uint64]EmptyStruct)
	var useServiceId = make(map[uint64]EmptyStruct)

	for _, service := range GlobalRPCServiceList {
		for _, mv := range service.MethodInfo {
			id, ok := ServiceIdMap[mv.KeyName()]
			if !ok {
				//Id文件未找到则是新消息,或者已经改名，新消息后面处理，这里不处理
				continue
			}
			if MessageIdFileMaxId < id {
				MessageIdFileMaxId = id
			}
			useServiceId[id] = EmptyStruct{}
			mv.Id = id
		}
	}

	for i := uint64(0); i < MaxMessageId; i++ {
		if _, ok := useServiceId[i]; !ok {
			unUseServiceId[i] = EmptyStruct{}
		}
	}

	for _, service := range GlobalRPCServiceList {
		for _, mv := range service.MethodInfo {
			if len(unUseServiceId) > 0 && mv.Id == math.MaxUint64 {
				for uk := range unUseServiceId {
					mv.Id = uk
					RpcIdMethodMap[mv.Id] = mv
					delete(unUseServiceId, uk)
					break
				}
				continue
			}
			if mv.Id == math.MaxUint64 {
				MessageIdFileMaxId++
				mv.Id = MessageIdFileMaxId
			}
			RpcIdMethodMap[mv.Id] = mv
			if FileMaxMessageId < mv.Id && mv.Id != math.MaxUint64 {
				FileMaxMessageId = mv.Id
			}
		}
	}
}

func GetProtocol(dirName string) uint32 {
	if utils.HasGrpcService(dirName) {
		return config.GrpcNode
	}
	return config.TcpNode
}

func GetProtocolByEnum(enumName string) uint32 {
	nodeName := strings.ReplaceAll(strings.ToLower(enumName), config.NodeServiceSuffix, "")
	for _, v := range config.ProtoDirectoryNames {
		if !strings.Contains(v, nodeName) || !strings.Contains(v, config.GrpcName) {
			continue
		}
		return config.GrpcNode
	}
	return config.TcpNode
}

func IsTcpNodeByEnum(dirName string) bool {
	return GetProtocolByEnum(dirName) == config.TcpNode
}

// writeServiceInfoCppFile generates C++ code that initializes gRPC service metadata.
func writeServiceInfoCppFile() {
	defer utils.Wg.Done()

	type ServiceInfoCppData struct {
		Includes             []string
		ServiceInfoIncludes  []string
		HandlerClasses       []string
		InitLines            []string
		ClientMessageIdLines []string
		MessageIdArraySize   int
		SenderFunctions      []string
	}

	const serviceInfoCppTemplate = `#include <array>
#include "service_metadata.h"
#include "proto/common/node.pb.h"

{{range .Includes -}}
{{ . }}
{{- end }}
{{range .ServiceInfoIncludes -}}
{{ . }}
{{- end }}
{{range .HandlerClasses}}
{{ . }}
{{- end }}
{{range .SenderFunctions}}
{{ . }}
{{- end }}

std::unordered_set<uint32_t> gClientMessageIdWhitelist;
std::array<RpcService, {{ .MessageIdArraySize }}> gRpcServiceRegistry;

void InitMessageInfo()
{
{{- range .InitLines }}
    {{ . }}
{{- end }}

{{range .ClientMessageIdLines }}
    {{ . }}
{{- end }}
}
`

	var (
		includes            []string
		serviceInfoIncludes []string
		handlerClasses      []string
		initLines           []string
		clientIdLines       []string
		senderFunction      []string
	)

	// Step 1: Collect headers and handler classes
	for _, service := range GlobalRPCServiceList {
		if len(service.MethodInfo) == 0 {
			continue
		}

		firstMethod := service.MethodInfo[0]

		if utils.IsPathInProtoDirs(firstMethod.Path(), config.DbProtoDirIndex) {
			continue
		}

		includes = append(includes, firstMethod.IncludeName())
		serviceInfoIncludes = append(serviceInfoIncludes, firstMethod.ServiceInfoIncludeName())

		if firstMethod.CcGenericServices() {

			handlerClass := fmt.Sprintf(
				"class %sImpl final : public %s {};",
				service.GetServiceName(),
				service.GetServiceName())
			handlerClasses = append(handlerClasses, handlerClass)
		}
	}

	// Step 2: Generate init lines for RpcService and allowed client message IDs
	for _, service := range GlobalRPCServiceList {
		if len(service.MethodInfo) == 0 {
			continue
		}

		firstMethod := service.MethodInfo[0]
		if utils.IsPathInProtoDirs(firstMethod.Path(), config.DbProtoDirIndex) {
			continue
		}

		for _, method := range service.MethodInfo {
			basePath := strings.ToLower(path.Base(method.Path()))
			messageId := method.KeyName() + config.MessageIdName

			isClientMessage := strings.Contains(service.GetServiceName(), config.ClientPrefixName)
			nodeType := fmt.Sprintf("eNodeType::%sNodeService", strcase.ToCamel(basePath))

			initLine := ""
			if method.CcGenericServices() {
				handler := service.GetServiceName() + "Impl"
				initLine = fmt.Sprintf(
					`gRpcServiceRegistry[%s] = RpcService{"%s", "%s", std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), %d, %s};`,
					messageId,
					method.Service(),
					method.Method(),
					method.CppRequest(),
					method.CppResponse(),
					handler,
					GetProtocol(method.Path()),
					nodeType,
				)
			} else {
				declareFunction := "namespace " + method.Package() + "{void Send" +
					service.GetServiceName() + method.MethodName() + "(entt::registry& , entt::entity , const google::protobuf::Message& , const std::vector<std::string>& , const std::vector<std::string>& );}"
				senderFunction = append(senderFunction, declareFunction)
				sendName := method.Package() + "::" + "Send" + service.GetServiceName() + method.MethodName()
				initLine = fmt.Sprintf(
					`gRpcServiceRegistry[%s] = RpcService{"%s", "%s", std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), nullptr, %d, %s, %s};`,
					messageId,
					method.Service(),
					method.Method(),
					method.CppRequest(),
					method.CppResponse(),
					GetProtocol(method.Path()),
					nodeType,
					sendName,
				)
			}

			initLines = append(initLines, initLine)

			if isClientMessage {
				clientIdLines = append(clientIdLines, fmt.Sprintf("gClientMessageIdWhitelist.emplace(%s);", messageId))
			}
		}
	}

	// Step 3: Fill template data and render
	tmplData := ServiceInfoCppData{
		Includes:             includes,
		ServiceInfoIncludes:  serviceInfoIncludes,
		HandlerClasses:       handlerClasses,
		InitLines:            initLines,
		ClientMessageIdLines: clientIdLines,
		MessageIdArraySize:   int(MessageIdLen()),
		SenderFunctions:      senderFunction,
	}

	tmpl, err := template.New("serviceInfoCpp").Parse(serviceInfoCppTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, tmplData); err != nil {
		panic(err)
	}

	WriteFileIfChanged(config.ServiceCppFilePath, output.Bytes())
}

// writeServiceInfoHeadFile writes service information to a header file.
func writeServiceInfoHeadFile() {
	defer utils.Wg.Done()
	type HeaderTemplateData struct {
		MaxMessageLen uint64
	}

	data := HeaderTemplateData{
		MaxMessageLen: MessageIdLen(),
	}

	err := RenderTemplateToFile("internal/template/service_header.tmpl", config.ServiceHeaderFilePath, data)
	if err != nil {
		panic(err)
		return
	}
}

// Helper function to generate instance data for player services.
func generateInstanceData(serviceList []string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir string, serviceName string) string {
	const playerInstanceTemplate = `#include <memory>
#include <string>
#include <unordered_map>
#include "{{ .SelfHeader }}"
{{- range .Includes }}
{{ . }}
{{- end }}

{{- range .HandlerClasses }}
{{ . }}
{{- end }}

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
{{- range .InitLines }}
    {{ . }}
{{- end }}
}
`
	type PlayerServiceInstanceData struct {
		SelfHeader     string   // e.g. login_player_service.h
		Includes       []string // list of #include "...Handler.h"
		HandlerClasses []string // e.g. class LoginServiceImpl : public LoginService {};
		InitLines      []string // e.g. g_player_service.emplace(...)
	}

	fileNameWithoutExt := serviceName[:len(serviceName)-len(filepath.Ext(serviceName))]

	var includes, handlerClasses, initLines []string

	for _, service := range GlobalRPCServiceList {
		if !isPlayerHandlerFunc(&service.MethodInfo) {
			continue
		}
		method := service.MethodInfo[0]
		className := method.Service() + "Impl"

		includes = append(includes, fmt.Sprintf(`#include "%s%s"`, method.FileBaseNameNoEx(), config.HandlerHeaderExtension))
		handlerClasses = append(handlerClasses, fmt.Sprintf("class %s : public %s {};", className, method.Service()))
		initLines = append(initLines, fmt.Sprintf(`gPlayerService.emplace("%s", std::make_unique<%s%s>(std::make_unique<%s>()));`,
			method.Service(), method.Service(), config.HandlerFileName, className))
	}

	data := PlayerServiceInstanceData{
		SelfHeader:     config.ServiceIncludeName + fileNameWithoutExt + ".h",
		Includes:       includes,
		HandlerClasses: handlerClasses,
		InitLines:      initLines,
	}

	tmpl, err := template.New("playerInstance").Parse(playerInstanceTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, data); err != nil {
		panic(err)
	}

	WriteFileIfChanged(handlerDir+serviceName, output.Bytes())
	return output.String()
}

// Helper function to generate instance data for player services.
func generateRepliedInstanceData(serviceList []string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir string, serviceName string) string {
	const repliedInstanceTemplate = `#include <memory>
#include <unordered_map>
#include "{{ .SelfHeader }}"
{{- range .Includes }}
{{ . }}
{{- end }}

{{- range .HandlerClasses }}
{{ . }}
{{- end }}

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
{{- range .InitLines }}
    {{ . }}
{{- end }}
}
`
	type PlayerServiceRepliedInstanceData struct {
		SelfHeader     string
		Includes       []string
		HandlerClasses []string
		InitLines      []string
	}

	fileNameWithoutExt := serviceName[:len(serviceName)-len(filepath.Ext(serviceName))]

	var includes, handlerClasses, initLines []string

	for _, service := range GlobalRPCServiceList {
		if !isPlayerHandlerFunc(&service.MethodInfo) {
			continue
		}

		method := service.MethodInfo[0]
		className := method.Service() + "Impl"

		includes = append(includes, fmt.Sprintf(`#include "%s%s"`, method.FileBaseNameNoEx(), config.RepliedHandlerHeaderExtension))
		handlerClasses = append(handlerClasses, fmt.Sprintf("class %s : public %s {};", className, method.Service()))
		initLines = append(initLines, fmt.Sprintf(`gPlayerServiceReplied.emplace("%s", std::make_unique<%s%s>(std::make_unique<%s>()));`,
			method.Service(), method.Service(), config.RepliedHandlerFileName, className))
	}

	templateData := PlayerServiceRepliedInstanceData{
		SelfHeader:     config.ServiceIncludeName + fileNameWithoutExt + ".h",
		Includes:       includes,
		HandlerClasses: handlerClasses,
		InitLines:      initLines,
	}

	tmpl, err := template.New("repliedInstance").Parse(repliedInstanceTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, templateData); err != nil {
		panic(err)
	}

	WriteFileIfChanged(handlerDir+serviceName, output.Bytes())
	return output.String()
}

func writePlayerServiceInstanceFiles(serviceType string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir, serviceName string) {
	defer utils.Wg.Done()
	ServiceList := make([]string, 0, len(GlobalRPCServiceList))

	for _, service := range GlobalRPCServiceList {
		ServiceList = append(ServiceList, service.GetServiceName())
	}

	var generatorFunc func([]string, func(*RPCMethods) bool, string, string) string

	if serviceType == "instance" {
		generatorFunc = generateInstanceData
	} else if serviceType == "repliedInstance" {
		generatorFunc = generateRepliedInstanceData
	} else {
		return // Handle error or unknown type
	}

	generatorFunc(ServiceList, isPlayerHandlerFunc, handlerDir, serviceName)
}

func WriteServiceRegisterInfoFile() {
	utils.Wg.Add(1)
	go writeServiceInfoCppFile()
	utils.Wg.Add(1)
	go writeServiceInfoHeadFile()
	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", IsRoomNodePlayerHandler, config.RoomNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", IsCentrePlayerServiceMethodHandler, config.CentreNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isRoomNodePlayerRepliedHandler, config.RoomNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", IsCentrePlayerServiceResponseHandler, config.CentrePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)

	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", ReturnNoHandler, config.GateNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	utils.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", ReturnNoHandler, config.GateNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
}
