// Package unity emits C# stubs that mirror the Go robot client handlers,
// allowing the Unity project to plug into server push messages with the same
// service+method naming convention used everywhere else in the pipeline.
package unity

import (
	"math"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"text/template"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/logger"

	"go.uber.org/zap"
	"google.golang.org/protobuf/reflect/protoreflect"
)

const (
	handlerTemplatePath  = "internal/template/unity_client_handler.cs.tmpl"
	registryTemplatePath = "internal/template/unity_client_handler_registry.cs.tmpl"
)

type handlerData struct {
	HandlerName  string
	ResponseType string
	MessageId    uint64
}

type registryData struct {
	Entries []handlerData
}

// UnityClientHandlerGenerator emits one C# partial-class stub per client RPC
// method into the configured Unity handler directory, plus a single registry
// file that wires every handler into a GameClient instance.
func UnityClientHandlerGenerator(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		if !_config.Global.Generators.EnableUnityClient {
			logger.Global.Debug("Unity client generator disabled, skipping")
			return
		}

		handlerDir := _config.Global.Paths.UnityHandlerDir
		registryFile := _config.Global.Paths.UnityHandlerRegistryFile
		if handlerDir == "" || registryFile == "" {
			logger.Global.Warn("Unity client paths not configured, skipping",
				zap.String("unity_handler_dir", handlerDir),
				zap.String("unity_handler_registry_file", registryFile),
			)
			return
		}

		if err := os.MkdirAll(handlerDir, 0o755); err != nil {
			logger.Global.Warn("Failed to create Unity handler directory",
				zap.String("dir", handlerDir),
				zap.Error(err),
			)
			return
		}
		if err := os.MkdirAll(filepath.Dir(registryFile), 0o755); err != nil {
			logger.Global.Warn("Failed to create Unity registry directory",
				zap.String("dir", filepath.Dir(registryFile)),
				zap.Error(err),
			)
			return
		}

		var entries []handlerData
		for _, service := range internal.GlobalRPCServiceList {
			if len(service.Methods) == 0 {
				continue
			}
			if !internal.IsClientProtocolService(service.Methods[0].ServiceDescriptorProto) {
				continue
			}
			for _, method := range service.Methods {
				if !isRelevantService(method) {
					continue
				}

				handlerName := method.Service() + method.Method() + "Handler"
				responseType, ok := csharpResponseType(method)
				if !ok {
					logger.Global.Debug("Unable to resolve C# response type, skipping",
						zap.String("service", method.Service()),
						zap.String("method", method.Method()),
					)
					continue
				}

				messageID := method.Id
				if messageID == 0 || messageID == math.MaxUint64 {
					logger.Global.Debug("No message id assigned, skipping",
						zap.String("service", method.Service()),
						zap.String("method", method.Method()),
					)
					continue
				}

				data := handlerData{
					HandlerName:  handlerName,
					ResponseType: responseType,
					MessageId:    messageID,
				}
				entries = append(entries, data)

				filePath := filepath.Join(handlerDir, handlerName+".cs")
				if err := writeHandlerFile(filePath, data); err != nil {
					logger.Global.Warn("Failed to write Unity handler file",
						zap.String("file", filePath),
						zap.Error(err),
					)
					continue
				}
			}
		}

		// Stable order so re-runs produce identical output.
		sort.Slice(entries, func(i, j int) bool {
			return entries[i].HandlerName < entries[j].HandlerName
		})

		if err := writeRegistryFile(registryFile, entries); err != nil {
			logger.Global.Warn("Failed to write Unity handler registry",
				zap.String("file", registryFile),
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Unity client handlers generated",
			zap.Int("handler_count", len(entries)),
			zap.String("handler_dir", handlerDir),
			zap.String("registry_file", registryFile),
		)
	}()
}

func writeHandlerFile(filePath string, data handlerData) error {
	if fileExists(filePath) {
		// Don't overwrite — generated files always live in a known directory
		// and the partial-method seam means the user file is separate.
		// We still want to rewrite when content changes; do so atomically.
	}

	tmpl, err := template.ParseFiles(handlerTemplatePath)
	if err != nil {
		return err
	}

	f, err := os.Create(filePath)
	if err != nil {
		return err
	}
	defer f.Close()
	return tmpl.Execute(f, data)
}

func writeRegistryFile(filePath string, entries []handlerData) error {
	tmpl, err := template.ParseFiles(registryTemplatePath)
	if err != nil {
		return err
	}

	f, err := os.Create(filePath)
	if err != nil {
		return err
	}
	defer f.Close()
	return tmpl.Execute(f, registryData{Entries: entries})
}

// isRelevantService keeps the same gating rule as the Go robot generator:
// only client-facing player services produce push handlers.
func isRelevantService(method *internal.MethodInfo) bool {
	svc := method.Service()
	return strings.Contains(svc, "GamePlayer") || strings.Contains(svc, "ClientPlayer")
}

// csharpResponseType returns the fully-qualified C# type for the method
// response (or, when the response is the empty marker, the request type --
// matching the Go robot generator's fallback). The C# namespace is derived
// from the proto package using protoc's default PascalCase convention.
func csharpResponseType(method *internal.MethodInfo) (string, bool) {
	resp := method.MethodDescriptorProto.GetOutputType()
	if name, ok := resolveCSharpType(resp); ok {
		if !isEmptyResponse(name) {
			return name, true
		}
	}
	req := method.MethodDescriptorProto.GetInputType()
	return resolveCSharpType(req)
}

func isEmptyResponse(csharpName string) bool {
	marker := _config.Global.Naming.EmptyResponse
	if marker == "" {
		return false
	}
	return strings.Contains(csharpName, marker)
}

// resolveCSharpType maps a fully-qualified proto type (e.g. ".loginpb.LoginC2LResponse")
// to the C# type name protoc-gen-csharp would emit (e.g. "Loginpb.LoginC2LResponse").
// Top-level types (no proto package) are returned bare so they resolve via the
// global namespace.
func resolveCSharpType(fullType string) (string, bool) {
	trimmed := strings.TrimPrefix(fullType, ".")
	if trimmed == "" {
		return "", false
	}

	desc, ok := internal.ActiveMsgDescCache[protoreflect.FullName(trimmed)]
	if !ok {
		return "", false
	}

	pkg := string(desc.ParentFile().Package())
	name := string(desc.Name())
	if pkg == "" {
		return name, true
	}
	return protoPackageToCSharpNamespace(pkg) + "." + name, true
}

// protoPackageToCSharpNamespace mirrors protoc's default mapping:
// each dot-separated segment is PascalCased.
func protoPackageToCSharpNamespace(pkg string) string {
	parts := strings.Split(pkg, ".")
	for i, p := range parts {
		if p == "" {
			continue
		}
		runes := []rune(p)
		runes[0] = upper(runes[0])
		parts[i] = string(runes)
	}
	return strings.Join(parts, ".")
}

func upper(r rune) rune {
	if r >= 'a' && r <= 'z' {
		return r - ('a' - 'A')
	}
	return r
}

func fileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	return err == nil
}
