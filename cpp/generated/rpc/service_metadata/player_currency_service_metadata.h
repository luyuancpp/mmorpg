#pragma once
#include <cstdint>

#include "proto/scene/player_currency.pb.h"

constexpr uint32_t SceneCurrencyClientPlayerGmAddCurrencyMessageId = 37;
constexpr uint32_t SceneCurrencyClientPlayerGmAddCurrencyIndex = 0;
#define SceneCurrencyClientPlayerGmAddCurrencyMethod  ::SceneCurrencyClientPlayer_Stub::descriptor()->method(0)

constexpr uint32_t SceneCurrencyClientPlayerGmDeductCurrencyMessageId = 49;
constexpr uint32_t SceneCurrencyClientPlayerGmDeductCurrencyIndex = 1;
#define SceneCurrencyClientPlayerGmDeductCurrencyMethod  ::SceneCurrencyClientPlayer_Stub::descriptor()->method(1)

constexpr uint32_t SceneCurrencyClientPlayerGetCurrencyListMessageId = 54;
constexpr uint32_t SceneCurrencyClientPlayerGetCurrencyListIndex = 2;
#define SceneCurrencyClientPlayerGetCurrencyListMethod  ::SceneCurrencyClientPlayer_Stub::descriptor()->method(2)

constexpr uint32_t SceneCurrencyClientPlayerGmBlockCurrencyMessageId = 94;
constexpr uint32_t SceneCurrencyClientPlayerGmBlockCurrencyIndex = 3;
#define SceneCurrencyClientPlayerGmBlockCurrencyMethod  ::SceneCurrencyClientPlayer_Stub::descriptor()->method(3)

constexpr uint32_t SceneCurrencyClientPlayerGmUnblockCurrencyMessageId = 95;
constexpr uint32_t SceneCurrencyClientPlayerGmUnblockCurrencyIndex = 4;
#define SceneCurrencyClientPlayerGmUnblockCurrencyMethod  ::SceneCurrencyClientPlayer_Stub::descriptor()->method(4)
