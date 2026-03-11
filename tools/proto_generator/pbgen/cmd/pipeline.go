package main

import (
	"sync"

	"pbgen/internal"
	cppGen "pbgen/internal/generator/cpp"
	cppGenOption "pbgen/internal/generator/cpp/options"
	goGen "pbgen/internal/generator/go"
	goGenOption "pbgen/internal/generator/go/options"
	"pbgen/internal/prototools"
	protoToolsOption "pbgen/internal/prototools/option"
)

func runGenerationPipeline(runner *ExecutionRunner) {
	runner.RunTask("MakeProjectDir", makeProjectDir)

	runner.RunSerialGroup("ProtoFilePreparation", []NamedTask{
		{Name: "prototools.CopyProtoToGenDir", Run: prototools.CopyProtoToGenDir},
		{Name: "goGen.AddGoPackageToProtoDir", Run: goGen.AddGoPackageToProtoDir},
	})

	runner.RunParallelGroup("GRPCGeneration", []NamedTask{
		{Name: "cppGen.GenerateGameGrpc", Run: cppGen.GenerateGameGrpc},
		{Name: "cppGen.ReadServiceIdFile", Run: cppGen.ReadServiceIdFile},
	})

	runner.RunTaskWithWG("prototools.GenerateAllInOneDescriptor", prototools.GenerateAllInOneDescriptor)
	runner.RunTaskWithWG("cppGen.ReadAllProtoFileServices", cppGen.ReadAllProtoFileServices)

	runner.RunParallelGroup("CompilationAndUtilGeneration", []NamedTask{
		{Name: "cppGen.BuildProtocCpp", Run: cppGen.BuildProtocCpp},
		{Name: "goGen.BuildGrpcServiceProto", Run: goGen.BuildGrpcServiceProto},
		{Name: "cppGen.GenNodeUtil", Run: cppGen.GenNodeUtil},
		{Name: "cppGen.GenerateAllEventHandlers", Run: cppGen.GenerateAllEventHandlers},
	})

	runner.RunTask("cppGen.InitServiceId", cppGen.InitServiceId)
	runner.RunTask("cppGen.WriteServiceIdFile", cppGen.WriteServiceIdFile)

	runner.RunParallelGroup("MethodFilesGeneration", []NamedTask{
		{Name: "cppGen.WriteMethodFile", Run: cppGen.WriteMethodFile},
		{Name: "cppGen.GeneratorHandler", Run: cppGen.GeneratorHandler},
	})

	runner.RunParallelGroup("ConstantsMessageIdsAndOptionBuilding", []NamedTask{
		{Name: "internal.GenerateServiceConstants", Run: internal.GenerateServiceConstants},
		{Name: "internal.WriteGoMessageId", Run: internal.WriteGoMessageId},
		{Name: "goGen.LoadAllDescriptors", Run: goGen.LoadAllDescriptors},
		{Name: "cppGenOption.BuildOption", Run: wrapNoWG(cppGenOption.BuildOption)},
		{Name: "goGenOption.BuildOption", Run: wrapNoWG(goGenOption.BuildOption)},
	})

	runner.RunParallelGroup("FinalTasksAndOptionProcessing", []NamedTask{
		{Name: "cppGen.WriteServiceRegisterInfoFile", Run: cppGen.WriteServiceRegisterInfoFile},
		{Name: "goGen.GenerateDBResource", Run: goGen.GenerateDBResource},
		{Name: "goGen.GoRobotHandlerGenerator", Run: goGen.GoRobotHandlerGenerator},
		{Name: "goGen.GoRobotTotalHandlerGenerator", Run: goGen.GoRobotTotalHandlerGenerator},
		{Name: "cppGen.CppPlayerDataLoadGenerator", Run: cppGen.CppPlayerDataLoadGenerator},
		{Name: "cppGen.CppGrpcCallClient", Run: cppGen.CppGrpcCallClient},
		{Name: "protoToolsOption.ProcessAllOptions", Run: func(wg *sync.WaitGroup) {
			protoToolsOption.ProcessAllOptions(wg, internal.FdSet)
		}},
	})
}
