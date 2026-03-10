package utils

import (
	"reflect"
	"testing"

	_config "pbgen/internal/config"
)

func TestProtoPathAndDomainHelpers(t *testing.T) {
	original := _config.Global
	t.Cleanup(func() {
		_config.Global = original
	})

	_config.Global.DomainMeta = map[string]_config.DomainMeta{
		"login": {
			Source: "login",
			Rpc:    _config.RpcMeta{Type: "grpc"},
		},
		"team": {
			Source: "team",
			Rpc:    _config.RpcMeta{Type: "both"},
		},
		"etcd": {
			Source: "etcd",
			Rpc:    _config.RpcMeta{Type: "none"},
		},
	}
	_config.Global.Paths.OutputRoot = "/tmp/out"
	_config.Global.Naming.Model = "model"
	_config.Global.DirectoryNames.GoGrpcBaseDirName = "service/go/grpc/"
	_config.Global.PathLists.ProtoDirectories = []string{
		"service/go/grpc/",
		"service/go/grpc/login",
		"service/go/grpc/team/",
		"service/go/rpc/db",
	}

	domain, ok := GetDomainByProtoPath("/repo/proto/login/account.proto")
	if !ok || domain != "login" {
		t.Fatalf("GetDomainByProtoPath mismatch: domain=%q ok=%v", domain, ok)
	}

	if !HasGrpcService("/repo/proto/login/account.proto") {
		t.Fatalf("expected grpc service for login proto")
	}
	if HasGrpcService("/repo/proto/unknown/account.proto") {
		t.Fatalf("did not expect grpc service for unknown proto")
	}
	if !HasEtcdService("/repo/proto/etcd/register.proto") {
		t.Fatalf("expected etcd service")
	}

	if got := BuildModelGoPath("/repo/proto/Player.proto"); got != "/tmp/out/go/player.proto/model" {
		t.Fatalf("BuildModelGoPath mismatch: %q", got)
	}

	grpcDirs := GetGRPCSubdirectories()
	expectedDirs := []string{"service/go/grpc/login/", "service/go/grpc/team/"}
	if !reflect.DeepEqual(grpcDirs, expectedDirs) {
		t.Fatalf("GetGRPCSubdirectories mismatch: got=%v want=%v", grpcDirs, expectedDirs)
	}

	grpcDomainNames := GetGRPCSubdirectoryNames()
	if len(grpcDomainNames) != 2 {
		t.Fatalf("GetGRPCSubdirectoryNames length mismatch: %v", grpcDomainNames)
	}
	nameSet := map[string]bool{}
	for _, n := range grpcDomainNames {
		nameSet[n] = true
	}
	if !nameSet["login"] || !nameSet["team"] {
		t.Fatalf("GetGRPCSubdirectoryNames missing expected names: %v", grpcDomainNames)
	}
}
