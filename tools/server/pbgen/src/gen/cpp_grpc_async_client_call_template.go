package gen

const AsyncClientHeaderTemplate = ` #pragma once

#include "proto/common/{{.FileBaseName}}.grpc.pb.h"
#include "proto/common/{{.FileBaseName}}.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;

{{- range .GrpcServices }}
class DeployAsync{{.Method}}ClientCall
{
public:
    ClientContext context;
    Status status;

    {{.Response}} reply;
    std::unique_ptr<ClientAsyncResponseReader< {{.Response}}>> response_reader;
};

{{- end }}`
