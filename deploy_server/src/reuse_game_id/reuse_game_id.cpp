#include "reuse_game_id.h"

namespace deploy
{

    void ReuseGSId::OnDbLoadComplete()
    {
        for (uint32_t i = 0; i < size(); ++i)
        {
            scan_node_ids_.emplace(i);
        }
    }

    void ReuseGSId::OnDisConnect(const std::string& ip)
    {
        auto it = gs_entities_.find(ip);
        if (it == gs_entities_.end())
        {
            return;
        }
        Destroy(it->second);
        gs_entities_.erase(it);
    }

    void ReuseGSId::Emplace(const std::string& ip, uint32_t id)
    {
        gs_entities_.emplace(ip, id);
        RemoveFree(id);
    }

    void ReuseGSId::ScanOver()
    {
        //等待一段时间后连接上来得先处理
        for (auto& it : gs_entities_)
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