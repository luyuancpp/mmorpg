package main

import (
	"sync"

	"protogen/internal"
	cppGen "protogen/internal/generator/cpp"
	cppGenOption "protogen/internal/generator/cpp/options"
	goGen "protogen/internal/generator/go"
	goGenOption "protogen/internal/generator/go/options"
	"protogen/internal/prototools"
	protoToolsOption "protogen/internal/prototools/option"
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
		{Name: "cppGen.ReadEventIdFile", Run: cppGen.ReadEventIdFile},
	})

	runner.RunTaskWithWG("prototools.GenerateAllInOneDescriptor", prototools.GenerateAllInOneDescriptor)
	runner.RunTaskWithWG("cppGen.ReadAllProtoFileServices", cppGen.ReadAllProtoFileServices)
	runner.RunTaskWithWG("cppGen.ReadAllProtoEvents", cppGen.ReadAllProtoEvents)

	runner.RunParallelGroup("CompilationAndUtilGeneration", []NamedTask{
		{Name: "cppGen.BuildProtocCpp", Run: cppGen.BuildProtocCpp},
		{Name: "goGen.BuildGrpcServiceProto", Run: goGen.BuildGrpcServiceProto},
		{Name: "cppGen.GenNodeUtil", Run: cppGen.GenNodeUtil},
		{Name: "cppGen.GenerateAllEventHandlers", Run: cppGen.GenerateAllEventHandlers},
		{Name: "cppGen.GenerateGateKafkaCommandRouter", Run: cppGen.GenerateGateKafkaCommandRouter},
	})

	runner.RunTask("cppGen.InitServiceId", cppGen.InitServiceId)
	runner.RunTask("cppGen.InitEventId", cppGen.InitEventId)
	runner.RunTask("cppGen.WriteServiceIdFile", cppGen.WriteServiceIdFile)
	runner.RunTask("cppGen.WriteEventIdFile", cppGen.WriteEventIdFile)

	runner.RunParallelGroup("MethodFilesGeneration", []NamedTask{
		{Name: "cppGen.WriteMethodFile", Run: cppGen.WriteMethodFile},
		{Name: "cppGen.GeneratorHandler", Run: cppGen.GeneratorHandler},
	})

	runner.RunParallelGroup("ConstantsMessageIdsAndOptionBuilding", []NamedTask{
		{Name: "internal.GenerateServiceConstants", Run: internal.GenerateServiceConstants},
		{Name: "internal.WriteGoMessageId", Run: internal.WriteGoMessageId},
		{Name: "internal.WriteGoEventId", Run: internal.WriteGoEventId},
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
