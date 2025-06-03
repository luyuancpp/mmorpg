package internal

import (
	"bufio"
	"bytes"
	"fmt"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/descriptorpb"
	"log"
	"math"
	"os"
	"path"
	"path/filepath"
	"pbgen/config"
	"pbgen/util"
	"sort"
	"strconv"
	"strings"
	"sync/atomic"
	"text/template"
)

// ReadProtoFileService reads service information from a protobuf descriptor file.
func ReadProtoFileService(fd os.DirEntry) error {

	// Check if the file is a proto file
	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	// Initialize the index for services in the file
	fileServiceIndex := uint32(0)

	// Construct the path to the descriptor file
	descFilePath := filepath.Join(config.PbDescDirectory, fd.Name()+config.ProtoDescExtension)

	// Read the descriptor file
	data, err := os.ReadFile(descFilePath)
	if err != nil {
		return fmt.Errorf("failed to read descriptor set file: %v", err)
	}

	// Unmarshal the descriptor set
	fdSet := &descriptorpb.FileDescriptorSet{}
	if err := proto.Unmarshal(data, fdSet); err != nil {
		return fmt.Errorf("failed to unmarshal descriptor set: %v", err)
	}

	// Iterate through each file in the descriptor set
	for _, file := range fdSet.File {
		for _, service := range file.Service {
			// Create an RPCServiceInfo object for each service
			rpcServiceInfo := RPCServiceInfo{
				FdSet:                  fdSet,
				FileServiceIndex:       fileServiceIndex,
				ServiceDescriptorProto: service,
			}

			// Iterate through each method in the service
			for index, method := range service.Method {
				// Create an MethodInfo object for each method
				rpcMethodInfo := MethodInfo{
					Id:                     math.MaxUint64,
					Index:                  uint64(index),
					FdSet:                  fdSet,
					ServiceDescriptorProto: service,
					MethodDescriptorProto:  method,
				}

				// Append the method info to the service info
				rpcServiceInfo.MethodInfo = append(rpcServiceInfo.MethodInfo, &rpcMethodInfo)

				// Increment the global message ID counter
				atomic.AddUint64(&MaxMessageId, 1)
			}

			// Store the service info in the global RpcServiceMap
			RpcServiceMap.Store(service.GetName(), &rpcServiceInfo)

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
			fileServiceIndex++
		}
	}

	// Return nil on successful completion
	return nil
}

// ReadProtoFileGrpcService reads gRPC service information from a protobuf file.
func ReadProtoFileGrpcService(fd os.DirEntry, filePath string) error {

	if !util.IsProtoFile(fd) {
		return fmt.Errorf("not a proto file: %s", fd.Name())
	}

	f, err := os.Open(filePath + fd.Name())
	if err != nil {
		return fmt.Errorf("failed to open file %s: %w", fd.Name(), err)
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	ccGenericServices := false

	for scanner.Scan() {
		line := scanner.Text()

		if strings.Contains(line, config.CcGenericServices) {
			ccGenericServices = true
		}

		if ccGenericServices {
			continue
		}

		if strings.Contains(line, "service ") && !strings.Contains(line, "=") {
			GrpcServiceFileMap.Store(fd.Name(), "")
			break
		}
	}

	if err := scanner.Err(); err != nil {
		return fmt.Errorf("error reading file %s: %w", fd.Name(), err)
	}

	return nil
}

// ReadAllProtoFileServices reads all service information from protobuf files in configured directories.
func ReadAllProtoFileServices() {
	for i := 0; i < len(config.ProtoDirs); i++ {
		fds, _ := os.ReadDir(config.ProtoDirs[i])
		for _, v := range fds {
			util.Wg.Add(1)
			fd := v
			go func(i int, fd os.DirEntry) {
				defer util.Wg.Done()
				_ = ReadProtoFileService(fd)
			}(i, fd)

			util.Wg.Add(1)
			go func(i int, fd os.DirEntry) {
				defer util.Wg.Done()
				_ = ReadProtoFileGrpcService(fd, config.ProtoDirs[i])
			}(i, fd)
		}
	}
}

// ReadServiceIdFile reads service IDs from a file asynchronously.
func ReadServiceIdFile() {
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

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
	util.Wg.Add(1)
	go func() {
		defer util.Wg.Done()

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
		util.WriteMd5Data2File(config.ServiceIdFilePath, data)
	}()
}

// InitServiceId initializes service IDs based on the loaded service methods and ID mappings.
func InitServiceId() {
	var unUseServiceId = make(map[uint64]EmptyStruct)
	var useServiceId = make(map[uint64]EmptyStruct)

	RpcServiceMap.Range(func(k, v interface{}) bool {
		key := k.(string)
		methodInfo := v.(*RPCServiceInfo).MethodInfo
		ServiceMethodMap[key] = methodInfo
		return true
	})

	for _, methodList := range ServiceMethodMap {
		for _, mv := range methodList {
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

	for _, methodList := range ServiceMethodMap {
		for _, mv := range methodList {
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

// GetSortServiceList returns a sorted list of service names.
func GetSortServiceList() []string {
	var ServiceList []string
	for k, _ := range ServiceMethodMap {
		ServiceList = append(ServiceList, k)
	}
	sort.Strings(ServiceList)
	return ServiceList
}

func GetProtocol(dirName string) uint32 {
	if config.GrpcServices[dirName] {
		return 1
	}
	return 0
}

// writeServiceInfoCppFile writes service information to a C++ file.
func writeServiceInfoCppFile() {
	type ServiceInfoCppData struct {
		Includes             []string
		ServiceInfoIncludes  []string
		HandlerClasses       []string
		InitLines            []string
		ClientMessageIdLines []string
		MessageIdArraySize   int
	}
	const serviceInfoCppTemplate = `#include <array>
#include "service_info.h"
#include "proto/common/node.pb.h"  

{{range .Includes -}}
{{ . }}
{{- end }}
{{- range .ServiceInfoIncludes -}}
{{ . -}}
{{- end }}
{{range .HandlerClasses }}
{{ . }}
{{- end }}

std::unordered_set<uint32_t> gAllowedClientMessageIds;
std::array<RpcService, {{ .MessageIdArraySize }}> gRpcServiceByMessageId;

void InitMessageInfo()
{
{{- range .InitLines }}
    {{ . }}
{{- end }}

{{- range .ClientMessageIdLines }}
    {{ . }}
{{- end }}
}
`

	defer util.Wg.Done()

	serviceList := GetSortServiceList()

	var includes, serviceInfoIncludes, handlerClasses, initLines, clientIdLines []string

	for _, serviceName := range serviceList {
		methods := ServiceMethodMap[serviceName]
		if len(methods) == 0 {
			continue
		}

		first := methods[0]

		if first.CcGenericServices() {
			includes = append(includes, first.IncludeName())
			serviceInfoIncludes = append(serviceInfoIncludes, first.ServiceInfoIncludeName())
			handlerClass := fmt.Sprintf("class %sImpl final : public %s {};", serviceName, serviceName)
			handlerClasses = append(handlerClasses, handlerClass)
			continue
		}

		includes = append(includes, first.GrpcIncludeHeadName())
		serviceInfoIncludes = append(serviceInfoIncludes, first.ServiceInfoIncludeName())
	}

	for _, serviceName := range serviceList {
		methods := ServiceMethodMap[serviceName]
		for _, method := range methods {
			basePath := strings.ToLower(path.Base(method.Path()))
			rpcId := method.KeyName() + config.MessageIdName

			if method.CcGenericServices() {
				handlerName := serviceName + "Impl"
				initLine := fmt.Sprintf(
					"gRpcServiceByMessageId[%s] = RpcService{\"%s\", \"%s\", std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), %d, eNodeType::%sNodeService};",
					rpcId,
					method.Service(),
					method.Method(),
					method.CppRequest(),
					method.CppResponse(),
					handlerName,
					GetProtocol(basePath),
					util.CapitalizeWords(basePath))
				initLines = append(initLines, initLine)

				if strings.Contains(serviceName, config.ClientPrefixName) {
					clientIdLines = append(clientIdLines, fmt.Sprintf("gAllowedClientMessageIds.emplace(%s);", rpcId))
				}
				continue
			}

			initLine := fmt.Sprintf(
				"gRpcServiceByMessageId[%s] = RpcService{\"%s\", \"%s\", std::make_unique_for_overwrite<%s>(), std::make_unique_for_overwrite<%s>(), nullptr, %d, eNodeType::%sNodeService};",
				rpcId,
				method.Service(),
				method.Method(),
				method.CppRequest(),
				method.CppResponse(),
				GetProtocol(basePath),
				util.CapitalizeWords(basePath))
			initLines = append(initLines, initLine)

			if strings.Contains(serviceName, config.ClientPrefixName) {
				clientIdLines = append(clientIdLines, fmt.Sprintf("gAllowedClientMessageIds.emplace(%s);", rpcId))
			}
		}
	}

	tmplData := ServiceInfoCppData{
		Includes:             includes,
		ServiceInfoIncludes:  serviceInfoIncludes,
		HandlerClasses:       handlerClasses,
		InitLines:            initLines,
		ClientMessageIdLines: clientIdLines,
		MessageIdArraySize:   int(MessageIdLen()),
	}

	tmpl, err := template.New("serviceInfoCpp").Parse(serviceInfoCppTemplate)
	if err != nil {
		panic(err)
	}

	var output bytes.Buffer
	if err := tmpl.Execute(&output, tmplData); err != nil {
		panic(err)
	}

	util.WriteMd5Data2File(config.ServiceCppFilePath, output.String())
}

// writeServiceInfoHeadFile writes service information to a header file.
func writeServiceInfoHeadFile() {
	defer util.Wg.Done()
	type HeaderTemplateData struct {
		MaxMessageLen uint64
	}

	data := HeaderTemplateData{
		MaxMessageLen: MessageIdLen(),
	}

	err := RenderTemplateToFile("internal/gen/template/service_header.tmpl", config.ServiceHeaderFilePath, data)
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

std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

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

	for _, key := range serviceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || !isPlayerHandlerFunc(&methodList) {
			continue
		}

		method := methodList[0]
		className := method.Service() + "Impl"

		includes = append(includes, fmt.Sprintf(`#include "%s%s"`, method.FileNameNoEx(), config.HandlerHeaderExtension))
		handlerClasses = append(handlerClasses, fmt.Sprintf("class %s : public %s {};", className, method.Service()))
		initLines = append(initLines, fmt.Sprintf(`g_player_service.emplace("%s", std::make_unique<%s%s>(std::make_unique<%s>()));`,
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

	util.WriteMd5Data2File(handlerDir+serviceName, output.String())
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

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

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

	for _, key := range serviceList {
		methodList, ok := ServiceMethodMap[key]
		if !ok || !isPlayerHandlerFunc(&methodList) {
			continue
		}

		method := methodList[0]
		className := method.Service() + "Impl"

		includes = append(includes, fmt.Sprintf(`#include "%s%s"`, method.FileNameNoEx(), config.RepliedHandlerHeaderExtension))
		handlerClasses = append(handlerClasses, fmt.Sprintf("class %s : public %s {};", className, method.Service()))
		initLines = append(initLines, fmt.Sprintf(`g_player_service_replied.emplace("%s", std::make_unique<%s%s>(std::make_unique<%s>()));`,
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

	util.WriteMd5Data2File(handlerDir+serviceName, output.String())
	return output.String()
}

func writePlayerServiceInstanceFiles(serviceType string, isPlayerHandlerFunc func(*RPCMethods) bool, handlerDir, serviceName string) {
	defer util.Wg.Done()
	ServiceList := GetSortServiceList()
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
	util.Wg.Add(1)
	go writeServiceInfoCppFile()
	util.Wg.Add(1)
	go writeServiceInfoHeadFile()
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", IsGsPlayerHandler, config.GameNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", isCentrePlayerHandler, config.CentreNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isGsPlayerRepliedHandler, config.GameNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", isCentrePlayerRepliedHandler, config.CentrePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)

	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("instance", ReturnNoHandler, config.GateNodePlayerMethodHandlerDirectory, config.PlayerServiceName)
	util.Wg.Add(1)
	go writePlayerServiceInstanceFiles("repliedInstance", ReturnNoHandler, config.GateNodePlayerMethodRepliedHandlerDirectory, config.PlayerRepliedServiceName)
}
