package prototools

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"sync"

	_config "protogen/internal/config"
	utils2 "protogen/internal/utils"
	"protogen/logger"

	"go.uber.org/zap"
)

// GenerateAllInOneDescriptor generates a merged Protobuf descriptor file.
func GenerateAllInOneDescriptor(wg *sync.WaitGroup) {
	wg.Add(1)

	go func() {
		defer wg.Done()
		if err := generateAllInOneDesc(); err != nil {
			logger.Global.Warn("Failed to generate descriptor",
				zap.Error(err),
			)
		}
	}()
}

// generateAllInOneDesc generates and parses the all-in-one descriptor file.
func generateAllInOneDesc() error {
	protoFiles, err := collectUniqueProtoFiles()
	if err != nil {
		logger.Global.Fatal("Failed to collect proto files",
			zap.Error(err),
		)
	}
	if len(protoFiles) == 0 {
		logger.Global.Info("Descriptor generation: no proto files found")
		return nil
	}
	logger.Global.Info("Descriptor generation: collected unique proto files",
		zap.Int("count", len(protoFiles)),
	)

	args, err := buildDescriptorArgs(protoFiles)
	if err != nil {
		logger.Global.Fatal("Failed to build command arguments",
			zap.Error(err),
		)
	}

	if err := executeDescriptorCommand(args); err != nil {
		logger.Global.Fatal("Failed to execute protoc",
			zap.Error(err),
		)
	}

	if err := parseDescriptorFile(); err != nil {
		logger.Global.Fatal("Failed to parse descriptor file",
			zap.Error(err),
		)
	}

	logger.Global.Info("Descriptor generation succeeded",
		zap.String("output_path", _config.Global.Paths.AllInOneDesc),
	)
	return nil
}

// collectUniqueProtoFiles collects all unique proto files across domains.
func collectUniqueProtoFiles() ([]string, error) {
	protoFileSet := make(map[string]struct{}) // deduplication set
	var allProtoFiles []string

	// map iteration order is random in Go; sort keys to stabilize generation output.
	domainKeys := make([]string, 0, len(_config.Global.DomainMeta))
	for key := range _config.Global.DomainMeta {
		domainKeys = append(domainKeys, key)
	}
	sort.Strings(domainKeys)

	for _, key := range domainKeys {
		meta := _config.Global.DomainMeta[key]
		fileEntries, err := os.ReadDir(meta.Source)
		if err != nil {
			logger.Global.Warn("Descriptor generation: failed to read directory, skipping",
				zap.String("dir", meta.Source),
				zap.Error(err),
			)
			continue
		}

		for _, entry := range fileEntries {
			if utils2.IsProtoFile(entry) {
				absPath, err := filepath.Abs(filepath.Join(meta.Source, entry.Name()))
				if err != nil {
					logger.Global.Warn("Descriptor generation: failed to get absolute path, skipping",
						zap.String("file_name", entry.Name()),
						zap.String("dir", meta.Source),
						zap.Error(err),
					)
					continue
				}
				absPath = filepath.ToSlash(absPath)

				if _, exists := protoFileSet[absPath]; !exists {
					protoFileSet[absPath] = struct{}{}
					allProtoFiles = append(allProtoFiles, absPath)
				}
			}
		}
	}

	// Keep protoc input order deterministic across machines/runs.
	sort.Strings(allProtoFiles)

	return allProtoFiles, nil
}

// buildDescriptorArgs builds protoc arguments for descriptor generation.
func buildDescriptorArgs(protoFiles []string) ([]string, error) {
	descOutput := filepath.ToSlash(_config.Global.Paths.AllInOneDesc)

	args := []string{
		"--descriptor_set_out=" + descOutput,
		"--include_imports",     // include all dependency descriptors
		"--include_source_info", // include source info for debugging
	}

	importPaths := []string{
		_config.Global.Paths.OutputRoot,
		_config.Global.Paths.ProtobufDir,
	}
	for _, ip := range importPaths {
		if ip == "" {
			continue // skip empty path
		}
		args = append(args, "--proto_path="+filepath.ToSlash(ip))
	}

	args = append(args, protoFiles...)
	return args, nil
}

// executeDescriptorCommand runs protoc to generate the descriptor file.
func executeDescriptorCommand(args []string) error {
	protocPath := "protoc" // use system protoc by default
	cmd := exec.Command(protocPath, args...)

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	logger.Global.Debug("Descriptor generation: executing protoc",
		zap.String("protoc_path", protocPath),
		zap.Strings("args", args),
	)
	if err := cmd.Run(); err != nil {
		logger.Global.Fatal("Command execution failed",
			zap.Error(err),
			zap.String("stderr", stderr.String()),
		)
	}

	if stdout.Len() > 0 {
		logger.Global.Info("Descriptor generation: command output",
			zap.String("stdout", stdout.String()),
		)
	}
	return nil
}
