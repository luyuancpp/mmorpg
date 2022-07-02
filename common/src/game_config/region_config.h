#ifndef COMMON_SRC_GAME_CONFIG_REGION_CONFIG_H_
#define COMMON_SRC_GAME_CONFIG_REGION_CONFIG_H_

#include "common.pb.h"

class RegionConfig
{
public:
    const RegionConfigInfo& config_info() const { return config_info_; }

    static RegionConfig& GetSingleton() {
        thread_local RegionConfig singleton;
        return singleton;
    }

    void Load(const std::string& filename);
private:
    RegionConfigInfo config_info_;
};
#endif // !COMMON_SRC_GAME_CONFIG_REGION_CONFIG_H_
