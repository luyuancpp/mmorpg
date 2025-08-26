#pragma once

#include <functional>  

using LoadSuccessCallback = std::function< void() >;

void LoadConfigs();

void LoadConfigsAsync();

void OnConfigLoadSuccess(const LoadSuccessCallback& callback);