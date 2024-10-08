﻿// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef COMMON_SRC_GAME_RPC_GAME_RPC_CHANEL_H_
#define COMMON_SRC_GAME_RPC_GAME_RPC_CHANEL_H_

#include "muduo/base/Atomic.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/protorpc/RpcCodec.h"

#include "network/codec/dispatcher.h"

#include <google/protobuf/service.h>

#include <map>

#include "game_rpc.pb.h"

// Service and RpcChannel classes are incorporated from
// google/protobuf/service.h

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

namespace google {
    namespace protobuf {

        // Defined in other files.
        class Descriptor;            // descriptor.h
        class ServiceDescriptor;     // descriptor.h
        class MethodDescriptor;      // descriptor.h
        class Message;               // message.h

        class Closure;

        class RpcController;
        class Service;

    }  // namespace protobuf
}  // namespace google


namespace muduo
{
    namespace net
    {

        // Abstract interface for an RPC channel.  An RpcChannel represents a
        // communication line to a Service which can be used to call that Service's
        // methods.  The Service may be running on another machine.  Normally, you
        // should not call an RpcChannel directly, but instead construct a stub Service
        // wrapping it.  Example:
        // FIXME: update here
        //   RpcChannel* channel = new MyRpcChannel("remotehost.example.com:1234");
        //   MyService* service = new MyService::Stub(channel);
        //   service->MyMethod(request, &response, callback);
        class RpcChannel 
        {
        public:
            RpcChannel();

            explicit RpcChannel(const TcpConnectionPtr& conn);

			~RpcChannel();

            void setConnection(const TcpConnectionPtr& conn)
            {
                conn_ = conn;
            }

            void setServices(const std::map<std::string, ::google::protobuf::Service*>* services)
            {
                services_ = services;
            }

            ProtobufDispatcher& protobufdispatcher() { return dispatcher_; }

            //rpc远程调用，回复rpc 的response
            void CallMethod(uint32_t message_id, const ::google::protobuf::Message& request);
            //发送到对应的服务器不回复
            void Send(uint32_t message_id, const ::google::protobuf::Message& message);
            //发送对应的串消息,
            void Route2Node(uint32_t message_id, const ::google::protobuf::Message& request);
            //返回串消息
            void SendRouteResponse(uint32_t message_id, uint64_t id, const std::string& message_bytes);

            void onMessage(const TcpConnectionPtr& conn,
                Buffer* buf,
                Timestamp receiveTime);

            void onUnknownMessage(const TcpConnectionPtr&,
                const MessagePtr& message,
                Timestamp);
        private:

            void SendRpcError(const RpcMessage& message, ErrorCode error);

            void onRpcMessage(const TcpConnectionPtr& conn,
                const RpcMessagePtr& messagePtr,
                Timestamp receiveTime);

            void onNormalRequestResponseMessage(const TcpConnectionPtr& conn,
                const RpcMessage& message,
                Timestamp receiveTime);

            void onRouteNodeMessage(const TcpConnectionPtr& conn,
                const RpcMessage& message,
                Timestamp receiveTime);

            void onS2CMessage(const TcpConnectionPtr& conn,
                const RpcMessage& message,
                Timestamp receiveTime);

            void MessageStatistics(const RpcMessage& message);

            void SendMessage(const RpcMessage& message);

            RpcCodec codec_;
            TcpConnectionPtr conn_;

            const std::map<std::string, ::google::protobuf::Service*>* services_;
            ProtobufDispatcher dispatcher_;
        };
        typedef std::shared_ptr<RpcChannel> RpcChannelPtr;

    }  // namespace net
}  // namespace muduo

#endif  // COMMON_SRC_GAME_RPC_GAME_RPC_CHANEL_H_
