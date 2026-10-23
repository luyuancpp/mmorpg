package _go

import (
	"bytes"
	"errors"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"text/template"
	"unicode"

	"protogen/internal"
	_config "protogen/internal/config"
	"protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
	"google.golang.org/protobuf/types/descriptorpb"
)

// robotHandlerTemplatePath 相对 protogen 根目录(生成器的工作目录)。
const robotHandlerTemplatePath = "internal/template/robot_handler.go.tmpl"

// robotHandlerReservedNames 是推送桩里固定占用的标识符:import 的 gameobject 与两个参数名。
var robotHandlerReservedNames = map[string]struct{}{"gameobject": {}, "player": {}, "response": {}}

// robotPushHandlerData 是 robot_handler.go.tmpl 的数据:一个 PUSH 方法的类型化桩。
type robotPushHandlerData struct {
	HandlerName string // <Service><Method>Handler,注册表按这个名字引用
	Alias       string // 载荷所在 proto 包的 import 别名
	ImportPath  string // 载荷所在 proto 包,例如 proto/guild
	PayloadType string // <Alias>.<Go 类型名>;PUSH 的载荷是方法输入类型
}

// GoRobotHandlerGenerator 为客户端协议服务的 PUSH 方法补齐 robot/logic/handler/<snake_service_method>.go 类型化桩
// (docs/design/client-rpc-callback.md §5.5)。
//
// 规则:
//   - 客户端协议服务只按 OptionIsClientProtocolService 判定(internal.ClassifyClientRpcServices),不看服务名;
//   - 只为 CLIENT_RPC_PUSH 建桩,文件已存在(手写或旧桩)即跳过,从不覆盖。推送 id = 0,只能按 message_id 派发;
//   - 推送 handler 写在标准文件名以外的文件里也可以:建桩前扫描目录里的包级函数(robotHandlerFuncFiles),
//     <Service><Method>Handler 已定义就不建桩,否则再建一个同名函数,robot 包重复定义编不过;
//   - REQUEST / ONE_WAY / SERVER_ONLY 不再建桩:robot 场景改用 robot/generated/rpc 的调用桩(请求自带回调)。
//     已有的手写 handler 与旧桩不删,注册表照常登记(GoRobotTotalHandlerGenerator),随场景迁移逐个退役。
//
// 与 GoRobotTotalHandlerGenerator 同组并行:注册表对 PUSH 按「本轮一定存在」登记,所以这里任何一个桩写失败都 Fatal。
func GoRobotHandlerGenerator(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		services, err := internal.ClassifyClientRpcServices(internal.GlobalRPCServiceList, internal.IsSceneTCPMethod)
		if err != nil {
			logger.Global.Fatal("Client RPC method kinds failed validation", zap.Error(err))
			return
		}

		dir := _config.Global.PathLists.MethodHandlerDirectories.Robot
		created, err := ensureRobotPushHandlers(services, internal.FdSet.GetFile(), dir, robotHandlerTemplatePath)
		if err != nil {
			logger.Global.Fatal("Failed to generate robot push handler stubs",
				zap.String("dir", dir),
				zap.Error(err),
			)
			return
		}

		logger.Global.Info("Robot push handler stubs checked",
			zap.Int("created_count", len(created)),
			zap.String("dir", dir),
		)
	}()
}

// ensureRobotPushHandlers 为缺少 handler 的 PUSH 方法在 dir 下建桩,返回新建文件的路径。已存在的文件一律不动;
// handler 已写在别的文件里(robotHandlerFuncFiles 找得到同名包级函数)时不建桩,注册表引用的就是那个函数。
// 错误:两个方法映射到同一个 handler 文件名(先于一切检查,一个桩都不建);以下收集全部后返回:载荷类型无法映射、
// 桩文件名会被 Go 工具链特殊对待、列目录 / 读文件 / stat 失败、模板或写盘失败。
func ensureRobotPushHandlers(services []internal.ClientRpcService, files []*descriptorpb.FileDescriptorProto,
	dir, templatePath string) ([]string, error) {
	tmpl, err := template.ParseFiles(templatePath)
	if err != nil {
		return nil, fmt.Errorf("parse template %s: %w", templatePath, err)
	}
	if err := checkRobotHandlerFileCollisions(services); err != nil {
		return nil, err
	}
	defined, err := robotHandlerFuncFiles(dir)
	if err != nil {
		return nil, err
	}
	types := newRobotGoTypeIndex(files)

	var (
		created []string
		errs    []error
	)
	for _, service := range services {
		for _, classified := range service.Methods {
			if classified.Kind != internal.ClientRpcPush {
				continue
			}
			method := classified.Method
			where := method.Service() + "." + method.Method()
			key := method.KeyName()
			fileName := robotHandlerFileName(key)
			if err := checkRobotGoFileName(fileName); err != nil {
				errs = append(errs, fmt.Errorf("%s: %w", where, err))
				continue
			}
			path := filepath.Join(dir, fileName)
			exists, err := fileExists(path)
			if err != nil {
				errs = append(errs, fmt.Errorf("%s: %w", where, err))
				continue
			}
			if exists {
				continue
			}
			handlerName := key + "Handler"
			if definedIn, ok := defined[handlerName]; ok {
				logger.Global.Info("Robot push handler is defined in another file; stub not generated",
					zap.String("handler_name", handlerName),
					zap.String("file_path", filepath.Join(dir, definedIn)),
				)
				continue
			}

			payload, err := types.resolve(method.MethodDescriptorProto.GetInputType())
			if err != nil {
				errs = append(errs, fmt.Errorf("%s: %w", where, err))
				continue
			}
			alias := robotImportAliases([]string{payload.ImportPath}, robotHandlerReservedNames)[payload.ImportPath]
			data := robotPushHandlerData{
				HandlerName: handlerName,
				Alias:       alias,
				ImportPath:  payload.ImportPath,
				PayloadType: alias + "." + payload.Name,
			}
			var content bytes.Buffer
			if err := tmpl.Execute(&content, data); err != nil {
				errs = append(errs, fmt.Errorf("%s: render %s: %w", where, fileName, err))
				continue
			}
			if err := utils.WriteFileIfChanged(path, content.Bytes()); err != nil {
				errs = append(errs, fmt.Errorf("%s: write %s: %w", where, path, err))
				continue
			}
			created = append(created, path)
			logger.Global.Info("Robot push handler stub generated",
				zap.String("file_path", path),
				zap.String("handler_name", data.HandlerName),
				zap.String("payload_type", data.PayloadType),
			)
		}
	}
	return created, errors.Join(errs...)
}

// robotHandlerFileName 是 <Service><Method> 的 handler 文件名;注册表判断 handler 是否存在、推送建桩都用它。
func robotHandlerFileName(key string) string {
	return camelToSnake(key) + ".go"
}

// fileExists checks if a file already exists.
func fileExists(filePath string) bool {
	_, err := os.Stat(filePath)
	if err != nil {
		if os.IsNotExist(err) {
			return false
		}
		logger.Global.Warn("Error checking file existence",
			zap.String("file_path", filePath),
			zap.Error(err),
		)
		return true
	}
	return true
}

// camelToSnake converts a CamelCase string to snake_case.
func camelToSnake(s string) string {
	var b strings.Builder
	runes := []rune(s)
	for i, r := range runes {
		if i > 0 && unicode.IsUpper(r) {
			prev := runes[i-1]
			if unicode.IsLower(prev) || unicode.IsDigit(prev) {
				b.WriteRune('_')
			} else if unicode.IsUpper(prev) && i+1 < len(runes) && unicode.IsLower(runes[i+1]) {
				b.WriteRune('_')
			}
		}
		b.WriteRune(unicode.ToLower(r))
	}
	return b.String()
}
