#pragma once
#include <cstdint>

#include "proto/scene/player_mystery_merchant.pb.h"

constexpr uint32_t SceneMysteryMerchantClientPlayerGetMysteryMerchantInfoMessageId = 314;
constexpr uint32_t SceneMysteryMerchantClientPlayerGetMysteryMerchantInfoIndex = 0;
#define SceneMysteryMerchantClientPlayerGetMysteryMerchantInfoMethod  ::SceneMysteryMerchantClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneMysteryMerchantClientPlayerBuyMysteryMerchantGoodsMessageId = 315;
constexpr uint32_t SceneMysteryMerchantClientPlayerBuyMysteryMerchantGoodsIndex = 1;
#define SceneMysteryMerchantClientPlayerBuyMysteryMerchantGoodsMethod  ::SceneMysteryMerchantClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneMysteryMerchantClientPlayerNotifyMysteryMerchantMessageId = 316;
constexpr uint32_t SceneMysteryMerchantClientPlayerNotifyMysteryMerchantIndex = 2;
#define SceneMysteryMerchantClientPlayerNotifyMysteryMerchantMethod  ::SceneMysteryMerchantClientPlayer_Stub::descriptor()->method(2)
