package server

import (
	"context"
	pb "deploy_server/pb/go"
	"fmt"
)

type DeployServer struct {
	pb.UnimplementedDeployServiceServer
}

func NewDeployServer() *DeployServer {
	return &DeployServer{}
}

func (gs *DeployServer) GetNodeInfo(ctx context.Context, req *pb.NodeInfoRequest) (*pb.NodeInfoResponse, error) {
	fmt.Println("=>", req)
	return &pb.NodeInfoResponse{}, nil
}

func (gs *DeployServer) StartGs(ctx context.Context, req *pb.StartGsRequest) (*pb.StartGsResponse, error) {
	fmt.Println("=>", req)
	return &pb.StartGsResponse{}, nil
}

func (gs *DeployServer) GetGsNodeId(ctx context.Context, req *pb.GsNodeIdRequest) (*pb.GsNodeIdResponse, error) {
	fmt.Println("=>", req)
	return &pb.GsNodeIdResponse{}, nil
}
