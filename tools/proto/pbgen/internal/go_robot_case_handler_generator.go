package internal

import (
	"fmt"
	"os"
	"strings"
	"text/template"

	"pbgen/config"
)

const handlerTotalTemplate = `package handler

import (
	"github.com/golang/protobuf/proto"
	"go.uber.org/zap"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func MessageBodyHandler(client *pkg.GameClient, response *game.MessageContent) {
	// Log the incoming message body for debugging
	zap.L().Debug("Received message body", zap.String("response", response.String()))

	// Retrieve the player from the player list
	player, ok := gameobject.PlayerList.Get(client.PlayerId)
	if !ok {
		zap.L().Error("Player not found", zap.Uint64("player_id", client.PlayerId))
		return
	}

	// Handle different message types
	switch response.MessageId {
	{{- range .Cases }}
	case game.{{.MessageID}}:
		{{.HandlerFunction}}(player, response.SerializedMessage)
	{{- end }}
	default:
		// Handle unknown message IDs
		zap.L().Info("Unhandled message", zap.Uint32("message_id", response.MessageId), zap.String("response", response.String()))
	}
}

{{- range .Cases }}
func {{.HandlerFunction}}(player *gameobject.Player, body []byte) {
	message := &game.{{.MessageType}}{}
	if err := proto.Unmarshal(body, message); err != nil {
		zap.L().Error("Failed to unmarshal {{.MessageType}}", zap.Error(err))
		return
	}
	{{.FunctionCall}}(player, message)
}

{{- end }}
`

type HandlerCase struct {
	MessageID       string
	HandlerFunction string
	MessageType     string
	FunctionCall    string
}

type CasesData struct {
	Cases []HandlerCase
}

func GoRobotTotalHandlerGenerator() {

	handlerCases := make([]HandlerCase, 0)

	ServiceList := GetSortServiceList()

	for _, key := range ServiceList {
		serviceMethods, ok := ServiceMethodMap[key]
		if !ok {
			continue
		}
		if !isClientMethodRepliedHandler(&serviceMethods) {
			continue
		}

		for _, method := range serviceMethods {

			if !isRelevantService(method) {
				continue
			}

			handlerCases = generateHandlerCases(method, handlerCases)
		}
	}

	err := generateTotalHandlerFile(config.RobotMessageBodyHandlerDirectory, handlerCases)
	if err != nil {
		return
	}
}

// generateHandlerCases creates the cases for the switch statement based on the method.
func generateHandlerCases(method *RPCMethod, cases []HandlerCase) []HandlerCase {
	handlerCases := HandlerCase{
		MessageID:       method.Service() + method.Method() + config.MessageIdName,
		HandlerFunction: "handle" + method.Service() + method.Method(),
		MessageType:     determineResponseType(method),
		FunctionCall:    method.Service() + method.Method() + "Handler",
	}
	cases = append(cases, handlerCases)
	return cases
}

// generateHandlerFile creates a new handler file with the specified parameters.
func generateTotalHandlerFile(fileName string, cases []HandlerCase) error {
	file, err := os.Create(fileName)
	if err != nil {
		return fmt.Errorf("could not create file %s: %w", fileName, err)
	}
	defer file.Close()

	tmpl, err := template.New("handler").Parse(handlerTotalTemplate)
	if err != nil {
		return fmt.Errorf("could not parse template: %w", err)
	}

	data := CasesData{
		Cases: cases,
	}

	if err := tmpl.Execute(file, data); err != nil {
		return fmt.Errorf("could not execute template: %w", err)
	}

	return nil
}

// isRelevantService checks if the service name is relevant.
func isRelevantService(method *RPCMethod) bool {
	return strings.Contains(method.Path(), config.ProtoDirectoryNames[config.ClientPlayerDirIndex]) ||
		strings.Contains(method.Service(), "GamePlayer") || strings.Contains(method.Service(), "ClientPlayer")
}

// determineResponseType returns the response type or request type based on configuration.
func determineResponseType(method *RPCMethod) string {
	if strings.Contains(method.CppResponse(), config.EmptyResponseName) {
		return method.CppRequest()
	}
	return method.CppResponse()
}
