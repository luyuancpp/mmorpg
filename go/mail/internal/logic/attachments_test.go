package logic

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"

	assetpb "proto/common/asset"
	mailpb "proto/mail"
)

// TestMailAttachmentShapesAlignWithAssetChannel:mail 自有的 MailCurrency / MailItem 与资产通道的 CurrencyAmount / ItemGrant
// 逐字段对齐(字段名、字段号、类型、个数都相同,设计 §3.5 / M-8)。通道侧加字段时这条变红,提醒 toAssetBundle 要跟着改 ——
// 这是"第二份形状"不漂移的唯一守法。
func TestMailAttachmentShapesAlignWithAssetChannel(t *testing.T) {
	for _, pair := range []struct {
		mail, channel protoreflect.MessageDescriptor
	}{
		{(&mailpb.MailCurrency{}).ProtoReflect().Descriptor(), (&assetpb.CurrencyAmount{}).ProtoReflect().Descriptor()},
		{(&mailpb.MailItem{}).ProtoReflect().Descriptor(), (&assetpb.ItemGrant{}).ProtoReflect().Descriptor()},
	} {
		mf, cf := pair.mail.Fields(), pair.channel.Fields()
		require.Equal(t, cf.Len(), mf.Len(), "%s 与 %s 字段个数不同", pair.mail.FullName(), pair.channel.FullName())
		for i := 0; i < cf.Len(); i++ {
			c := cf.Get(i)
			m := mf.ByName(c.Name())
			require.NotNil(t, m, "%s 缺字段 %s", pair.mail.FullName(), c.Name())
			assert.Equal(t, c.Number(), m.Number(), "%s.%s 字段号", pair.mail.FullName(), c.Name())
			assert.Equal(t, c.Kind(), m.Kind(), "%s.%s 类型", pair.mail.FullName(), c.Name())
			assert.Equal(t, c.Cardinality(), m.Cardinality(), "%s.%s 基数", pair.mail.FullName(), c.Name())
		}
	}
	// AssetBundle 里 mail 用到的两个列表也必须还在原位(字段号 1 / 2)。
	bundle := (&assetpb.AssetBundle{}).ProtoReflect().Descriptor().Fields()
	assert.Equal(t, protoreflect.FieldNumber(1), bundle.ByName("currencies").Number())
	assert.Equal(t, protoreflect.FieldNumber(2), bundle.ByName("items").Number())
}

func TestAttachmentsRoundTripSorted(t *testing.T) {
	in := &mailpb.MailAttachments{
		Currencies: []*mailpb.MailCurrency{{CurrencyType: 2, Amount: 5}, {CurrencyType: 0, Amount: 7}},
		Items:      []*mailpb.MailItem{{ConfigId: 9, Count: 1}, {ConfigId: 3, Count: 2}},
	}
	norm, err := normalizeAttachments(in, testConfig("dev").Mail.AllowsCurrency)
	require.NoError(t, err)
	b, err := encodeAttachments(norm)
	require.NoError(t, err)
	out, err := decodeAttachments(b)
	require.NoError(t, err)
	assert.Equal(t, []uint32{0, 2}, []uint32{out.GetCurrencies()[0].GetCurrencyType(), out.GetCurrencies()[1].GetCurrencyType()}, "按 currency_type 排序")
	assert.Equal(t, []uint32{3, 9}, []uint32{out.GetItems()[0].GetConfigId(), out.GetItems()[1].GetConfigId()}, "按 config_id 排序")
	assert.Equal(t, uint64(7), out.GetCurrencies()[0].GetAmount())

	// 存下来的字节就是 AssetBundle(M2 领取原样入账),不带 item_uuids / pet_id。
	bundle := &assetpb.AssetBundle{}
	require.NoError(t, proto.Unmarshal(b, bundle))
	assert.Len(t, bundle.GetCurrencies(), 2)
	assert.Empty(t, bundle.GetItemUuids())
	assert.Zero(t, bundle.GetPetId())

	// 原请求不被就地改写(normalize 返回副本)。
	assert.Equal(t, uint32(2), in.GetCurrencies()[0].GetCurrencyType())
}

func TestEmptyAttachments(t *testing.T) {
	for _, a := range []*mailpb.MailAttachments{nil, {}} {
		norm, err := normalizeAttachments(a, testConfig("dev").Mail.AllowsCurrency)
		require.NoError(t, err)
		assert.Nil(t, norm)
		b, err := encodeAttachments(norm)
		require.NoError(t, err)
		assert.Empty(t, b, "没有附件 = 空列(claim_state 推导为 0)")
	}
	out, err := decodeAttachments(nil)
	require.NoError(t, err)
	assert.Nil(t, out)
}

func TestDecodeRejectsForeignBundles(t *testing.T) {
	_, err := decodeAttachments([]byte{0xff, 0xff, 0xff})
	assert.Error(t, err, "解不开 = 数据损坏")
	withPet, err := proto.Marshal(&assetpb.AssetBundle{PetId: 3})
	require.NoError(t, err)
	_, err = decodeAttachments(withPet)
	assert.Error(t, err, "mail 从不写 pet_id,出现了只能是行被别的东西改写过")
	withUUIDs, err := proto.Marshal(&assetpb.AssetBundle{ItemUuids: []uint64{1}})
	require.NoError(t, err)
	_, err = decodeAttachments(withUUIDs)
	assert.Error(t, err)
}
