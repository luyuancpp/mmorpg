#include "reuse_game_id.h"

namespace deploy
{

    void ReuseGameServerId::OnDbLoadComplete()
    {
        for (uint32_t i = 0; i < size(); ++i)
        {
            scan_node_ids_.emplace(i);
        }
    }

    void ReuseGameServerId::Emplace(const std::string& ip, uint32_t id)
    {
        game_entities_.emplace(ip, id);
        RemoveFree(id);
    }

    void ReuseGameServerId::OnDisConnect(const std::string& ip)
    {
        auto it = game_entities_.find(ip);
        if (it == game_entities_.end())
        {
            return;
        }
        Destroy(it->second);
        game_entities_.erase(it);
    }

    void ReuseGameServerId::EraseScanEmpty(uint32_t id)
    {
        scan_node_ids_.erase(id);
    }

    void ReuseGameServerId::ScanOver()
    {
        //等待一段时间后连接上来得先处理
        for (auto& it : game_entities_)
        {
            scan_node_ids_.erase(it.second);
        }
        //超过一段时间后，没有连接上来得id 重用
        for (auto& it : scan_node_ids_)
        {
            Destroy(it);
        }
        scan_node_ids_.clear();
    }

}//namespace deploy