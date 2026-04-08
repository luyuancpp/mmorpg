package _go

import (
	"os"
	"strings"
	"sync"
	"text/template"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
)

const handlerTotalTemplate = `package handler

import (
	"reflect"

	"go.uber.org/zap"
	"google.golang.org/protobuf/proto"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/pkg"
	base "proto/common/base"
)

type handlerFunc func(*gameobject.Player, []byte)

// unmarshalAndCall creates a handlerFunc that unmarshals body into a new
// message of type PT and forwards it to the typed handler function.
func unmarshalAndCall[PT proto.Message](fn func(*gameobject.Player, PT)) handlerFunc {
	var zero PT
	msgType := reflect.TypeOf(zero).Elem()
	return func(player *gameobject.Player, body []byte) {
		msg := reflect.New(msgType).Interface().(PT)
		if err := proto.Unmarshal(body, msg); err != nil {
			zap.L().Error("unmarshal failed", zap.Error(err))
			return
		}
		fn(player, msg)
	}
}

var messageHandlers = map[uint32]handlerFunc{
{{- range .Cases }}
	game.{{.MessageID}}: unmarshalAndCall({{.FunctionCall}}),
{{- end }}
}

func MessageBodyHandler(client *pkg.GameClient, response *base.MessageContent) {
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	player, ok := gameobject.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	if h, ok := messageHandlers[response.MessageId]; ok {
		h(player, response.SerializedMessage)
	} else {
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId))
	}
}
`

type HandlerCase struct {
	MessageID    string
	FunctionCall string
}

type CasesData struct {
	Cases []HandlerCase
}

// isClientMethodRepliedHandler checks if this is a client method replied handler.
func isClientMethodRepliedHandler(methodList *internal.RPCMethods) bool {
	firstMethodInfo := (*methodList)[0]
	return internal.IsClientProtocolService(firstMethodInfo.ServiceDescriptorProto)
}

func GoRobotTotalHandlerGenerator(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		handlerCases := make([]HandlerCase, 0)
		for _, service := range internal.GlobalRPCServiceList {
			if !isClientMethodRepliedHandler(&service.Methods) {
				logger.Global.Debug("Skipping non-client-response service",
					zap.String("service_name", service.Service()),
				)
				continue
			}

			for _, method := range service.Methods {
				if !isRelevantService(method) {
					logger.Global.Debug("Skipping irrelevant service method",
						zap.String("service_name", method.Service()),
						zap.String("method_name", method.Method()),
					)
					continue
				}
				handlerCases = generateHandlerCases(method, handlerCases)
			}
		}

		logger.Global.Info("Generating robot message handler file",
			zap.Int("handler_case_count", len(handlerCases)),
		)

		err := generateTotalHandlerFile(_config.Global.Paths.RobotMsgBodyHandlerFile, handlerCases)
		if err != nil {
			logger.Global.Error("Failed to generate robot message handler file",
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Robot message handler file generated",
			zap.String("file_path", _config.Global.Paths.RobotMsgBodyHandlerFile),
			zap.Int("handler_case_count", len(handlerCases)),
		)
	}()
}

// generateHandlerCases creates the cases for the handler map based on the method.
func generateHandlerCases(method *internal.MethodInfo, cases []HandlerCase) []HandlerCase {
	handlerCase := HandlerCase{
		MessageID:    method.Service() + method.Method() + _config.Global.Naming.MessageId,
		FunctionCall: method.Service() + method.Method() + "Handler",
	}

	logger.Global.Debug("Generated handler case",
		zap.String("service_name", method.Service()),
		zap.String("method_name", method.Method()),
		zap.String("message_id", handlerCase.MessageID),
		zap.String("function_call", handlerCase.FunctionCall),
	)

	cases = append(cases, handlerCase)
	return cases
}

// generateHandlerFile creates a new handler file with the specified parameters.
func generateTotalHandlerFile(fileName string, cases []HandlerCase) error {
	if fileName == "" {
		logger.Global.Fatal("Failed to generate handler file: file path is empty")
	}

	file, err := os.Create(fileName)
	if err != nil {
		logger.Global.Fatal("Failed to create handler file",
			zap.String("file_name", fileName),
			zap.Error(err),
		)
	}
	defer file.Close()

	tmpl, err := template.New("handler").Parse(handlerTotalTemplate)
	if err != nil {
		logger.Global.Fatal("Failed to parse handler template",
			zap.Error(err),
		)
	}

	data := CasesData{
		Cases: cases,
	}

	if err := tmpl.Execute(file, data); err != nil {
		logger.Global.Fatal("Failed to execute handler template",
			zap.String("file_name", fileName),
			zap.Int("case_count", len(cases)),
			zap.Error(err),
		)
	}

	logger.Global.Debug("Handler template executed",
		zap.String("file_name", fileName),
		zap.Int("case_count", len(cases)),
	)

	return nil
}

// isRelevantService checks if the service name is relevant.
func isRelevantService(method *internal.MethodInfo) bool {
	isRelevant := strings.Contains(method.Service(), "GamePlayer") || strings.Contains(method.Service(), "ClientPlayer")

	if !isRelevant {
		logger.Global.Debug("Service method is irrelevant, skipping",
			zap.String("service_name", method.Service()),
			zap.String("method_name", method.Method()),
		)
	}

	return isRelevant
}
