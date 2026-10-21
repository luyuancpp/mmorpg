package accountdirectory

import (
	"testing"

	base "proto/common/base"
	dspb "proto/data_service"

	"google.golang.org/protobuf/proto"
)

func TestRoleAdmissionIdentityAndEvidenceAreStrict(t *testing.T) {
	i := &base.RoleAdmissionIdentity{PlayerId: ^uint64(0), SceneInstanceId: "11111111-2222-3333-4444-555555555555", Generation: 1, OwnerRevision: 7, OwnerEpoch: 17, SceneNodeId: "11", SceneId: 9, ZoneId: 1, SessionId: 5, LoginGeneration: 99}
	if !ValidAdmission(i) {
		t.Fatal("有效入场身份被拒")
	}
	for _, mutate := range []func(*base.RoleAdmissionIdentity){func(i *base.RoleAdmissionIdentity) { i.OwnerRevision = 0 }, func(i *base.RoleAdmissionIdentity) { i.SceneInstanceId = "AAAAAAAA-2222-3333-4444-555555555555" }, func(i *base.RoleAdmissionIdentity) { i.Generation = 0 }, func(i *base.RoleAdmissionIdentity) { i.SceneNodeId = "11\n" }, func(i *base.RoleAdmissionIdentity) { i.OwnerEpoch = 0 }} {
		copy := proto.Clone(i).(*base.RoleAdmissionIdentity)
		mutate(copy)
		if ValidAdmission(copy) {
			t.Fatalf("非法身份被接受: %v", copy)
		}
	}
	e := &dspb.AccountRoleFreezeEvidence{PlayerId: 1, ListingId: 2, OwnerRevision: 3, PersistedDigest: make([]byte, 32), Kind: dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_NO_ADMISSION}
	if !ValidFreezeEvidence(e) {
		t.Fatal("显式无入场证据被拒")
	}
	e.Kind = dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_UNSPECIFIED
	if ValidFreezeEvidence(e) {
		t.Fatal("未分类0/0证据被接受")
	}
	e.Kind = dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_SCENE
	if ValidFreezeEvidence(e) {
		t.Fatal("SCENE证据允许0/0")
	}
	e.OwnerEpoch, e.SaveSeq = 17, 3
	if !ValidFreezeEvidence(e) {
		t.Fatal("已分类SCENE证据被拒")
	}
	e.Kind = dspb.AccountRoleFreezeEvidenceKind_ACCOUNT_ROLE_FREEZE_EVIDENCE_KIND_NO_ADMISSION
	if ValidFreezeEvidence(e) {
		t.Fatal("NO_ADMISSION证据允许非零围栏")
	}
}
