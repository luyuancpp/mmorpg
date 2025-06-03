#pragma once
#include "proto/scene/player_state_attribute_sync.pb.h"


class EntitySyncServiceHandler : public ::PlayerSyncService
{
public:


	void SyncBaseAttribute(::google::protobuf::RpcController* controller,
		const ::BaseAttributeSyncDataS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;



	void SyncAttribute2Frames(::google::protobuf::RpcController* controller,
		const ::AttributeDelta2FramesS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;



	void SyncAttribute5Frames(::google::protobuf::RpcController* controller,
		const ::AttributeDelta5FramesS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;



	void SyncAttribute10Frames(::google::protobuf::RpcController* controller,
		const ::AttributeDelta10FramesS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;



	void SyncAttribute30Frames(::google::protobuf::RpcController* controller,
		const ::AttributeDelta30FramesS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;



	void SyncAttribute60Frames(::google::protobuf::RpcController* controller,
		const ::AttributeDelta60FramesS2C* request,
		::Empty* response,
		::google::protobuf::Closure* done) override;


};