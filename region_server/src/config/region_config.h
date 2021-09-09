#ifndef REGION_SERVER_SRC_CONFIG_REGION_CONFIG_H_
#define REGION_SERVER_SRC_CONFIG_REGION_CONFIG_H_

#include "common.pb.h"

namespace region
{
class RegionConfig
{
public:
    const RegionConfigInfo& config_info() const { return config_info_; }

    static RegionConfig& GetSingleton() {
        static RegionConfig singleton;
        return singleton;
    }

    void Load(const std::string& filename);
private:
    RegionConfigInfo config_info_;
};
}//namespace region
#endif // !REGION_SERVER_SRC_CONFIG_REGION_CONFIG_H_
