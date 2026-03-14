package cpp

import (
	"bufio"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"

	_config "pbgen/internal/config"
	utils2 "pbgen/internal/utils"
	"pbgen/logger"

	"go.uber.org/zap"
)

type ProtoEventInfo struct {
	Id            uint64
	IdName        string
	MessageName   string
	QualifiedName string
	ProtoInclude  string
}

var (
	globalProtoEventList []*ProtoEventInfo
	eventIdNameMap       = map[string]uint64{}
	eventIdInfoMap       = map[uint64]*ProtoEventInfo{}
	eventFileMaxId       uint64
)

func buildEventIdName(packageName, messageName string) string {
	parts := make([]string, 0)
	for _, part := range strings.Split(packageName, ".") {
		if part == "" {
			continue
		}
		parts = append(parts, strings.ToUpper(part[:1])+part[1:])
	}
	parts = append(parts, messageName)
	return strings.Join(parts, "")
}

func registerProtoEvents(protoRelativeDir string, files []os.DirEntry) {
	for _, file := range files {
		protoFilePath := filepath.Join(_config.Global.Paths.ProtoDir, protoRelativeDir, file.Name())
		messages, err := parseProtoMessages(protoFilePath)
		if err != nil {
			logger.Global.Fatal("解析event proto失败",
				zap.String("proto_file", protoFilePath),
				zap.Error(err),
			)
		}

		packageName, err := parseProtoPackage(protoFilePath)
		if err != nil {
			logger.Global.Fatal("解析event proto package失败",
				zap.String("proto_file", protoFilePath),
				zap.Error(err),
			)
		}

		protoInclude := filepath.ToSlash(filepath.Join(
			_config.Global.DirectoryNames.ProtoDirName,
			protoRelativeDir,
			strings.Replace(file.Name(), _config.Global.FileExtensions.Proto, _config.Global.FileExtensions.PbH, 1),
		))

		for _, messageName := range messages {
			idName := buildEventIdName(packageName, messageName)
			for _, existing := range globalProtoEventList {
				if existing.IdName == idName {
					logger.Global.Fatal("event ID 名称冲突",
						zap.String("event_name", idName),
						zap.String("qualified_name", qualifyProtoType(packageName, messageName)),
					)
				}
			}

			globalProtoEventList = append(globalProtoEventList, &ProtoEventInfo{
				Id:            math.MaxUint64,
				IdName:        idName,
				MessageName:   messageName,
				QualifiedName: qualifyProtoType(packageName, messageName),
				ProtoInclude:  protoInclude,
			})
		}
	}
}

func ReadAllProtoEvents(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		logicEventFiles, err := readProtoFiles(_config.Global.PathLists.ProtoDirs.LogicEvent)
		if err != nil {
			logger.Global.Fatal("读取逻辑event proto目录失败",
				zap.String("dir", _config.Global.PathLists.ProtoDirs.LogicEvent),
				zap.Error(err),
			)
		}
		registerProtoEvents(_config.Global.PathLists.ProtoDirs.LogicEvent, filterProtoFiles(logicEventFiles))

		if _config.Global.PathLists.ProtoDirs.ContractsKafka == "" {
			return
		}

		kafkaEventFiles, err := readProtoFiles(_config.Global.PathLists.ProtoDirs.ContractsKafka)
		if err != nil {
			logger.Global.Warn("读取Kafka event proto目录失败，跳过Kafka event ID生成",
				zap.String("dir", _config.Global.PathLists.ProtoDirs.ContractsKafka),
				zap.Error(err),
			)
			return
		}
		registerProtoEvents(_config.Global.PathLists.ProtoDirs.ContractsKafka, filterProtoFilesBySuffix(kafkaEventFiles, "_event.proto"))

		sort.Slice(globalProtoEventList, func(i, j int) bool {
			return globalProtoEventList[i].IdName < globalProtoEventList[j].IdName
		})
	}()
}

func ReadEventIdFile(wg *sync.WaitGroup) {
	wg.Add(1)
	go func() {
		defer wg.Done()

		file, err := os.Open(_config.Global.Paths.EventIdFile)
		if err != nil {
			if os.IsNotExist(err) {
				return
			}
			logger.Global.Fatal("读取event ID文件失败",
				zap.String("file_path", _config.Global.Paths.EventIdFile),
				zap.Error(err),
			)
		}
		defer file.Close()

		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			line := scanner.Text()
			parts := strings.Split(line, "=")
			if len(parts) != 2 {
				logger.Global.Warn("event ID文件格式错误，跳过无效行",
					zap.String("line", line),
				)
				continue
			}

			id, convErr := strconv.ParseUint(strings.TrimSpace(parts[0]), 10, 64)
			if convErr != nil {
				logger.Global.Fatal("解析event ID失败",
					zap.String("line", line),
					zap.Error(convErr),
				)
			}

			eventIdNameMap[strings.TrimSpace(parts[1])] = id
			if eventFileMaxId < id {
				eventFileMaxId = id
			}
		}

		if err = scanner.Err(); err != nil {
			logger.Global.Fatal("扫描event ID文件失败",
				zap.String("file_path", _config.Global.Paths.EventIdFile),
				zap.Error(err),
			)
		}
	}()
}

func InitEventId() {
	usedIds := map[uint64]struct{}{}
	availableIds := make([]uint64, 0)

	for _, event := range globalProtoEventList {
		if id, ok := eventIdNameMap[event.IdName]; ok {
			event.Id = id
			eventIdInfoMap[id] = event
			usedIds[id] = struct{}{}
		}
	}

	for id := uint64(0); id <= eventFileMaxId; id++ {
		if _, ok := usedIds[id]; ok {
			continue
		}
		availableIds = append(availableIds, id)
	}

	nextIndex := 0
	for _, event := range globalProtoEventList {
		if event.Id != math.MaxUint64 {
			continue
		}

		if nextIndex < len(availableIds) {
			event.Id = availableIds[nextIndex]
			nextIndex++
		} else if len(eventIdInfoMap) == 0 && eventFileMaxId == 0 {
			event.Id = 0
		} else {
			eventFileMaxId++
			event.Id = eventFileMaxId
		}
		if eventFileMaxId < event.Id {
			eventFileMaxId = event.Id
		}
		eventIdInfoMap[event.Id] = event
	}
}

func WriteEventIdFile() {
	ids := make([]uint64, 0, len(eventIdInfoMap))
	for id := range eventIdInfoMap {
		ids = append(ids, id)
	}
	sort.Slice(ids, func(i, j int) bool { return ids[i] < ids[j] })

	var builder strings.Builder
	for _, id := range ids {
		builder.WriteString(fmt.Sprintf("%d=%s\n", id, eventIdInfoMap[id].IdName))
	}

	utils2.WriteFileIfChanged(_config.Global.Paths.EventIdFile, []byte(builder.String()))
}

func EventIdLen() uint64 {
	if len(eventIdInfoMap) == 0 {
		return 0
	}
	return eventFileMaxId + 1
}
