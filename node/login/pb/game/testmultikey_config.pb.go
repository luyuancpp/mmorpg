// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.34.2
// 	protoc        v5.26.1
// source: testmultikey_config.proto

package game

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type Tag1 struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Tag1Key   string `protobuf:"bytes,1,opt,name=tag1_key,json=tag1Key,proto3" json:"tag1_key,omitempty"`
	Tag1Value string `protobuf:"bytes,2,opt,name=tag1_value,json=tag1Value,proto3" json:"tag1_value,omitempty"`
}

func (x *Tag1) Reset() {
	*x = Tag1{}
	if protoimpl.UnsafeEnabled {
		mi := &file_testmultikey_config_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Tag1) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Tag1) ProtoMessage() {}

func (x *Tag1) ProtoReflect() protoreflect.Message {
	mi := &file_testmultikey_config_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Tag1.ProtoReflect.Descriptor instead.
func (*Tag1) Descriptor() ([]byte, []int) {
	return file_testmultikey_config_proto_rawDescGZIP(), []int{0}
}

func (x *Tag1) GetTag1Key() string {
	if x != nil {
		return x.Tag1Key
	}
	return ""
}

func (x *Tag1) GetTag1Value() string {
	if x != nil {
		return x.Tag1Value
	}
	return ""
}

type Immunetag1 struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Immunetag1Key   string `protobuf:"bytes,1,opt,name=immunetag1_key,json=immunetag1Key,proto3" json:"immunetag1_key,omitempty"`
	Immunetag1Value string `protobuf:"bytes,2,opt,name=immunetag1_value,json=immunetag1Value,proto3" json:"immunetag1_value,omitempty"`
}

func (x *Immunetag1) Reset() {
	*x = Immunetag1{}
	if protoimpl.UnsafeEnabled {
		mi := &file_testmultikey_config_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Immunetag1) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Immunetag1) ProtoMessage() {}

func (x *Immunetag1) ProtoReflect() protoreflect.Message {
	mi := &file_testmultikey_config_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Immunetag1.ProtoReflect.Descriptor instead.
func (*Immunetag1) Descriptor() ([]byte, []int) {
	return file_testmultikey_config_proto_rawDescGZIP(), []int{1}
}

func (x *Immunetag1) GetImmunetag1Key() string {
	if x != nil {
		return x.Immunetag1Key
	}
	return ""
}

func (x *Immunetag1) GetImmunetag1Value() string {
	if x != nil {
		return x.Immunetag1Value
	}
	return ""
}

type Testobj1 struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Testobj1Key   uint32 `protobuf:"varint,1,opt,name=testobj1_key,json=testobj1Key,proto3" json:"testobj1_key,omitempty"`
	Testobj1Value uint32 `protobuf:"varint,2,opt,name=testobj1_value,json=testobj1Value,proto3" json:"testobj1_value,omitempty"`
}

func (x *Testobj1) Reset() {
	*x = Testobj1{}
	if protoimpl.UnsafeEnabled {
		mi := &file_testmultikey_config_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Testobj1) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Testobj1) ProtoMessage() {}

func (x *Testobj1) ProtoReflect() protoreflect.Message {
	mi := &file_testmultikey_config_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Testobj1.ProtoReflect.Descriptor instead.
func (*Testobj1) Descriptor() ([]byte, []int) {
	return file_testmultikey_config_proto_rawDescGZIP(), []int{2}
}

func (x *Testobj1) GetTestobj1Key() uint32 {
	if x != nil {
		return x.Testobj1Key
	}
	return 0
}

func (x *Testobj1) GetTestobj1Value() uint32 {
	if x != nil {
		return x.Testobj1Value
	}
	return 0
}

type TestMultiKeyTablePB struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Id         uint32            `protobuf:"varint,1,opt,name=id,proto3" json:"id,omitempty"`
	Tag1       map[string]string `protobuf:"bytes,2,rep,name=tag1,proto3" json:"tag1,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Immunetag1 map[string]string `protobuf:"bytes,3,rep,name=immunetag1,proto3" json:"immunetag1,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Level      uint32            `protobuf:"varint,4,opt,name=level,proto3" json:"level,omitempty"`
	Testobj1   []*Testobj1       `protobuf:"bytes,5,rep,name=testobj1,proto3" json:"testobj1,omitempty"`
	Effect     []uint32          `protobuf:"varint,6,rep,packed,name=effect,proto3" json:"effect,omitempty"`
	Stringkey  string            `protobuf:"bytes,7,opt,name=stringkey,proto3" json:"stringkey,omitempty"`
	Uint32Key  uint32            `protobuf:"varint,8,opt,name=uint32key,proto3" json:"uint32key,omitempty"`
	In32Key    int32             `protobuf:"varint,9,opt,name=in32key,proto3" json:"in32key,omitempty"`
	Mstringkey string            `protobuf:"bytes,10,opt,name=mstringkey,proto3" json:"mstringkey,omitempty"`
	Muint32Key uint32            `protobuf:"varint,11,opt,name=muint32key,proto3" json:"muint32key,omitempty"`
	Min32Key   int32             `protobuf:"varint,12,opt,name=min32key,proto3" json:"min32key,omitempty"`
}

func (x *TestMultiKeyTablePB) Reset() {
	*x = TestMultiKeyTablePB{}
	if protoimpl.UnsafeEnabled {
		mi := &file_testmultikey_config_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *TestMultiKeyTablePB) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*TestMultiKeyTablePB) ProtoMessage() {}

func (x *TestMultiKeyTablePB) ProtoReflect() protoreflect.Message {
	mi := &file_testmultikey_config_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use TestMultiKeyTablePB.ProtoReflect.Descriptor instead.
func (*TestMultiKeyTablePB) Descriptor() ([]byte, []int) {
	return file_testmultikey_config_proto_rawDescGZIP(), []int{3}
}

func (x *TestMultiKeyTablePB) GetId() uint32 {
	if x != nil {
		return x.Id
	}
	return 0
}

func (x *TestMultiKeyTablePB) GetTag1() map[string]string {
	if x != nil {
		return x.Tag1
	}
	return nil
}

func (x *TestMultiKeyTablePB) GetImmunetag1() map[string]string {
	if x != nil {
		return x.Immunetag1
	}
	return nil
}

func (x *TestMultiKeyTablePB) GetLevel() uint32 {
	if x != nil {
		return x.Level
	}
	return 0
}

func (x *TestMultiKeyTablePB) GetTestobj1() []*Testobj1 {
	if x != nil {
		return x.Testobj1
	}
	return nil
}

func (x *TestMultiKeyTablePB) GetEffect() []uint32 {
	if x != nil {
		return x.Effect
	}
	return nil
}

func (x *TestMultiKeyTablePB) GetStringkey() string {
	if x != nil {
		return x.Stringkey
	}
	return ""
}

func (x *TestMultiKeyTablePB) GetUint32Key() uint32 {
	if x != nil {
		return x.Uint32Key
	}
	return 0
}

func (x *TestMultiKeyTablePB) GetIn32Key() int32 {
	if x != nil {
		return x.In32Key
	}
	return 0
}

func (x *TestMultiKeyTablePB) GetMstringkey() string {
	if x != nil {
		return x.Mstringkey
	}
	return ""
}

func (x *TestMultiKeyTablePB) GetMuint32Key() uint32 {
	if x != nil {
		return x.Muint32Key
	}
	return 0
}

func (x *TestMultiKeyTablePB) GetMin32Key() int32 {
	if x != nil {
		return x.Min32Key
	}
	return 0
}

type TestMultiKeyTabledDataPB struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Data []*TestMultiKeyTablePB `protobuf:"bytes,1,rep,name=data,proto3" json:"data,omitempty"`
}

func (x *TestMultiKeyTabledDataPB) Reset() {
	*x = TestMultiKeyTabledDataPB{}
	if protoimpl.UnsafeEnabled {
		mi := &file_testmultikey_config_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *TestMultiKeyTabledDataPB) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*TestMultiKeyTabledDataPB) ProtoMessage() {}

func (x *TestMultiKeyTabledDataPB) ProtoReflect() protoreflect.Message {
	mi := &file_testmultikey_config_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use TestMultiKeyTabledDataPB.ProtoReflect.Descriptor instead.
func (*TestMultiKeyTabledDataPB) Descriptor() ([]byte, []int) {
	return file_testmultikey_config_proto_rawDescGZIP(), []int{4}
}

func (x *TestMultiKeyTabledDataPB) GetData() []*TestMultiKeyTablePB {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_testmultikey_config_proto protoreflect.FileDescriptor

var file_testmultikey_config_proto_rawDesc = []byte{
	0x0a, 0x19, 0x74, 0x65, 0x73, 0x74, 0x6d, 0x75, 0x6c, 0x74, 0x69, 0x6b, 0x65, 0x79, 0x5f, 0x63,
	0x6f, 0x6e, 0x66, 0x69, 0x67, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x22, 0x40, 0x0a, 0x04, 0x74,
	0x61, 0x67, 0x31, 0x12, 0x19, 0x0a, 0x08, 0x74, 0x61, 0x67, 0x31, 0x5f, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x74, 0x61, 0x67, 0x31, 0x4b, 0x65, 0x79, 0x12, 0x1d,
	0x0a, 0x0a, 0x74, 0x61, 0x67, 0x31, 0x5f, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x09, 0x52, 0x09, 0x74, 0x61, 0x67, 0x31, 0x56, 0x61, 0x6c, 0x75, 0x65, 0x22, 0x5e, 0x0a,
	0x0a, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31, 0x12, 0x25, 0x0a, 0x0e, 0x69,
	0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31, 0x5f, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20,
	0x01, 0x28, 0x09, 0x52, 0x0d, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31, 0x4b,
	0x65, 0x79, 0x12, 0x29, 0x0a, 0x10, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31,
	0x5f, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0f, 0x69, 0x6d,
	0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31, 0x56, 0x61, 0x6c, 0x75, 0x65, 0x22, 0x54, 0x0a,
	0x08, 0x74, 0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x12, 0x21, 0x0a, 0x0c, 0x74, 0x65, 0x73,
	0x74, 0x6f, 0x62, 0x6a, 0x31, 0x5f, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52,
	0x0b, 0x74, 0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x4b, 0x65, 0x79, 0x12, 0x25, 0x0a, 0x0e,
	0x74, 0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x5f, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x0d, 0x74, 0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x56, 0x61,
	0x6c, 0x75, 0x65, 0x22, 0x9e, 0x04, 0x0a, 0x13, 0x54, 0x65, 0x73, 0x74, 0x4d, 0x75, 0x6c, 0x74,
	0x69, 0x4b, 0x65, 0x79, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x50, 0x42, 0x12, 0x0e, 0x0a, 0x02, 0x69,
	0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x02, 0x69, 0x64, 0x12, 0x32, 0x0a, 0x04, 0x74,
	0x61, 0x67, 0x31, 0x18, 0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x1e, 0x2e, 0x54, 0x65, 0x73, 0x74,
	0x4d, 0x75, 0x6c, 0x74, 0x69, 0x4b, 0x65, 0x79, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x50, 0x42, 0x2e,
	0x54, 0x61, 0x67, 0x31, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x04, 0x74, 0x61, 0x67, 0x31, 0x12,
	0x44, 0x0a, 0x0a, 0x69, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74, 0x61, 0x67, 0x31, 0x18, 0x03, 0x20,
	0x03, 0x28, 0x0b, 0x32, 0x24, 0x2e, 0x54, 0x65, 0x73, 0x74, 0x4d, 0x75, 0x6c, 0x74, 0x69, 0x4b,
	0x65, 0x79, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x50, 0x42, 0x2e, 0x49, 0x6d, 0x6d, 0x75, 0x6e, 0x65,
	0x74, 0x61, 0x67, 0x31, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x0a, 0x69, 0x6d, 0x6d, 0x75, 0x6e,
	0x65, 0x74, 0x61, 0x67, 0x31, 0x12, 0x14, 0x0a, 0x05, 0x6c, 0x65, 0x76, 0x65, 0x6c, 0x18, 0x04,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x05, 0x6c, 0x65, 0x76, 0x65, 0x6c, 0x12, 0x25, 0x0a, 0x08, 0x74,
	0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x18, 0x05, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x09, 0x2e,
	0x74, 0x65, 0x73, 0x74, 0x6f, 0x62, 0x6a, 0x31, 0x52, 0x08, 0x74, 0x65, 0x73, 0x74, 0x6f, 0x62,
	0x6a, 0x31, 0x12, 0x16, 0x0a, 0x06, 0x65, 0x66, 0x66, 0x65, 0x63, 0x74, 0x18, 0x06, 0x20, 0x03,
	0x28, 0x0d, 0x52, 0x06, 0x65, 0x66, 0x66, 0x65, 0x63, 0x74, 0x12, 0x1c, 0x0a, 0x09, 0x73, 0x74,
	0x72, 0x69, 0x6e, 0x67, 0x6b, 0x65, 0x79, 0x18, 0x07, 0x20, 0x01, 0x28, 0x09, 0x52, 0x09, 0x73,
	0x74, 0x72, 0x69, 0x6e, 0x67, 0x6b, 0x65, 0x79, 0x12, 0x1c, 0x0a, 0x09, 0x75, 0x69, 0x6e, 0x74,
	0x33, 0x32, 0x6b, 0x65, 0x79, 0x18, 0x08, 0x20, 0x01, 0x28, 0x0d, 0x52, 0x09, 0x75, 0x69, 0x6e,
	0x74, 0x33, 0x32, 0x6b, 0x65, 0x79, 0x12, 0x18, 0x0a, 0x07, 0x69, 0x6e, 0x33, 0x32, 0x6b, 0x65,
	0x79, 0x18, 0x09, 0x20, 0x01, 0x28, 0x05, 0x52, 0x07, 0x69, 0x6e, 0x33, 0x32, 0x6b, 0x65, 0x79,
	0x12, 0x1e, 0x0a, 0x0a, 0x6d, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x6b, 0x65, 0x79, 0x18, 0x0a,
	0x20, 0x01, 0x28, 0x09, 0x52, 0x0a, 0x6d, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x6b, 0x65, 0x79,
	0x12, 0x1e, 0x0a, 0x0a, 0x6d, 0x75, 0x69, 0x6e, 0x74, 0x33, 0x32, 0x6b, 0x65, 0x79, 0x18, 0x0b,
	0x20, 0x01, 0x28, 0x0d, 0x52, 0x0a, 0x6d, 0x75, 0x69, 0x6e, 0x74, 0x33, 0x32, 0x6b, 0x65, 0x79,
	0x12, 0x1a, 0x0a, 0x08, 0x6d, 0x69, 0x6e, 0x33, 0x32, 0x6b, 0x65, 0x79, 0x18, 0x0c, 0x20, 0x01,
	0x28, 0x05, 0x52, 0x08, 0x6d, 0x69, 0x6e, 0x33, 0x32, 0x6b, 0x65, 0x79, 0x1a, 0x37, 0x0a, 0x09,
	0x54, 0x61, 0x67, 0x31, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79,
	0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76,
	0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75,
	0x65, 0x3a, 0x02, 0x38, 0x01, 0x1a, 0x3d, 0x0a, 0x0f, 0x49, 0x6d, 0x6d, 0x75, 0x6e, 0x65, 0x74,
	0x61, 0x67, 0x31, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x3a, 0x02, 0x38, 0x01, 0x22, 0x44, 0x0a, 0x18, 0x54, 0x65, 0x73, 0x74, 0x4d, 0x75, 0x6c, 0x74,
	0x69, 0x4b, 0x65, 0x79, 0x54, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x44, 0x61, 0x74, 0x61, 0x50, 0x42,
	0x12, 0x28, 0x0a, 0x04, 0x64, 0x61, 0x74, 0x61, 0x18, 0x01, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x14,
	0x2e, 0x54, 0x65, 0x73, 0x74, 0x4d, 0x75, 0x6c, 0x74, 0x69, 0x4b, 0x65, 0x79, 0x54, 0x61, 0x62,
	0x6c, 0x65, 0x50, 0x42, 0x52, 0x04, 0x64, 0x61, 0x74, 0x61, 0x42, 0x09, 0x5a, 0x07, 0x70, 0x62,
	0x2f, 0x67, 0x61, 0x6d, 0x65, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_testmultikey_config_proto_rawDescOnce sync.Once
	file_testmultikey_config_proto_rawDescData = file_testmultikey_config_proto_rawDesc
)

func file_testmultikey_config_proto_rawDescGZIP() []byte {
	file_testmultikey_config_proto_rawDescOnce.Do(func() {
		file_testmultikey_config_proto_rawDescData = protoimpl.X.CompressGZIP(file_testmultikey_config_proto_rawDescData)
	})
	return file_testmultikey_config_proto_rawDescData
}

var file_testmultikey_config_proto_msgTypes = make([]protoimpl.MessageInfo, 7)
var file_testmultikey_config_proto_goTypes = []any{
	(*Tag1)(nil),                     // 0: tag1
	(*Immunetag1)(nil),               // 1: immunetag1
	(*Testobj1)(nil),                 // 2: testobj1
	(*TestMultiKeyTablePB)(nil),      // 3: TestMultiKeyTablePB
	(*TestMultiKeyTabledDataPB)(nil), // 4: TestMultiKeyTabledDataPB
	nil,                              // 5: TestMultiKeyTablePB.Tag1Entry
	nil,                              // 6: TestMultiKeyTablePB.Immunetag1Entry
}
var file_testmultikey_config_proto_depIdxs = []int32{
	5, // 0: TestMultiKeyTablePB.tag1:type_name -> TestMultiKeyTablePB.Tag1Entry
	6, // 1: TestMultiKeyTablePB.immunetag1:type_name -> TestMultiKeyTablePB.Immunetag1Entry
	2, // 2: TestMultiKeyTablePB.testobj1:type_name -> testobj1
	3, // 3: TestMultiKeyTabledDataPB.data:type_name -> TestMultiKeyTablePB
	4, // [4:4] is the sub-list for method output_type
	4, // [4:4] is the sub-list for method input_type
	4, // [4:4] is the sub-list for extension type_name
	4, // [4:4] is the sub-list for extension extendee
	0, // [0:4] is the sub-list for field type_name
}

func init() { file_testmultikey_config_proto_init() }
func file_testmultikey_config_proto_init() {
	if File_testmultikey_config_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_testmultikey_config_proto_msgTypes[0].Exporter = func(v any, i int) any {
			switch v := v.(*Tag1); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_testmultikey_config_proto_msgTypes[1].Exporter = func(v any, i int) any {
			switch v := v.(*Immunetag1); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_testmultikey_config_proto_msgTypes[2].Exporter = func(v any, i int) any {
			switch v := v.(*Testobj1); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_testmultikey_config_proto_msgTypes[3].Exporter = func(v any, i int) any {
			switch v := v.(*TestMultiKeyTablePB); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_testmultikey_config_proto_msgTypes[4].Exporter = func(v any, i int) any {
			switch v := v.(*TestMultiKeyTabledDataPB); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_testmultikey_config_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   7,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_testmultikey_config_proto_goTypes,
		DependencyIndexes: file_testmultikey_config_proto_depIdxs,
		MessageInfos:      file_testmultikey_config_proto_msgTypes,
	}.Build()
	File_testmultikey_config_proto = out.File
	file_testmultikey_config_proto_rawDesc = nil
	file_testmultikey_config_proto_goTypes = nil
	file_testmultikey_config_proto_depIdxs = nil
}
