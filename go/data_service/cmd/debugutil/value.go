package debugutil

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"unicode/utf8"

	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"
)

// Value 是导出文件里一个二进制值的统一呈现:原字节(base64,供 debug_import 逐字节还原)+ 人能读的解码结果。
// debug_fetch 的旧模式与 player-bundle 共用这一个形状,JSON 键名是既有导出文件的契约,不要改名。
type Value struct {
	Kind        string `json:"kind"`
	Size        int    `json:"size"`
	RawBase64   string `json:"raw_base64,omitempty"`
	Text        string `json:"text,omitempty"`
	JSONValue   any    `json:"json,omitempty"`
	ProtoType   string `json:"proto_type,omitempty"`
	ProtoJSON   any    `json:"proto_json,omitempty"`
	DecodeError string `json:"decode_error,omitempty"`
}

// RawBytes 还原 RawBase64;没有原字节(空值或刻意不导出的脱敏值)时返回 (nil, false)。
func (v *Value) RawBytes() ([]byte, bool, error) {
	if v == nil || v.RawBase64 == "" {
		return nil, false, nil
	}
	raw, err := base64.StdEncoding.DecodeString(v.RawBase64)
	if err != nil {
		return nil, false, fmt.Errorf("decode raw_base64: %w", err)
	}
	return raw, true, nil
}

// ProtoJSONOf 把消息转成通用 JSON 值。uint64 按 protojson 规则输出为字符串,
// 避免 jq / 浏览器把 2^53 以上的 player_id、item guid 舍入成别的数。
func ProtoJSONOf(msg proto.Message) (any, error) {
	raw, err := protojson.MarshalOptions{EmitUnpopulated: true, UseProtoNames: true}.Marshal(msg)
	if err != nil {
		return nil, err
	}
	var out any
	if err := json.Unmarshal(raw, &out); err != nil {
		return string(raw), nil
	}
	return out, nil
}

// RenderProto 按调用方给定的类型解码。类型由键清单 / 表结构显式给出,不按名字猜。
// redact 非 nil 时先对解码结果脱敏,并且**不输出原字节**:脱敏值只供查看,不能被导回。
func RenderProto(data []byte, newMsg func() proto.Message, redact func(proto.Message)) Value {
	out := Value{Size: len(data)}
	if len(data) == 0 {
		out.Kind = "text"
		return out
	}
	if redact == nil {
		out.RawBase64 = base64.StdEncoding.EncodeToString(data)
	}
	msg := newMsg()
	out.ProtoType = string(msg.ProtoReflect().Descriptor().FullName())
	if err := proto.Unmarshal(data, msg); err != nil {
		out.Kind = "binary"
		out.DecodeError = fmt.Sprintf("unmarshal %s: %v", out.ProtoType, err)
		return out
	}
	if redact != nil {
		redact(msg)
	}
	value, err := ProtoJSONOf(msg)
	if err != nil {
		out.Kind = "binary"
		out.DecodeError = fmt.Sprintf("marshal %s to json: %v", out.ProtoType, err)
		return out
	}
	out.Kind = "protobuf"
	out.ProtoJSON = value
	return out
}

// RenderText 用于本来就是文本的值(owner_epoch、版本令牌、账号名);不是合法 UTF-8 时退回二进制。
func RenderText(data []byte) Value {
	out := Value{Size: len(data)}
	if len(data) == 0 {
		out.Kind = "text"
		return out
	}
	out.RawBase64 = base64.StdEncoding.EncodeToString(data)
	if utf8.Valid(data) {
		out.Kind = "text"
		out.Text = string(data)
		return out
	}
	out.Kind = "binary"
	return out
}
