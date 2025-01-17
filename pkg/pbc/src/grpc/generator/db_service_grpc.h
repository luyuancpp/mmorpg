#pragma once

#include "proto/common/db_service.grpc.pb.h"
#include "proto/common/db_service.pb.h"

using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
class AsyncAccountDBServiceLoad2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    LoadAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< LoadAccountResponse>> response_reader;
};

class LoadAccountRequest;
void AccountDBServiceLoad2Redis(const LoadAccountRequest& request);
class AsyncAccountDBServiceSave2RedisGrpcClientCall
{
public:
    ClientContext context;
    Status status;

    SaveAccountResponse reply;
    std::unique_ptr<ClientAsyncResponseReader< SaveAccountResponse>> response_reader;
};

class SaveAccountRequest;
void AccountDBServiceSave2Redis(const SaveAccountRequest& request);