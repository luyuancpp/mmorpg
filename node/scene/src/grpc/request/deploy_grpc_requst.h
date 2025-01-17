#pragma once

class NodeInfoRequest;
void SendGetNodeInfo(const NodeInfoRequest& rq);

class GetIDRequest;
void GetID(const GetIDRequest& request);

class ReleaseIDRequest;
void ReleaseID(const ReleaseIDRequest& request);

class RenewLeaseIDRequest;
void RenewLease(const RenewLeaseIDRequest& request);