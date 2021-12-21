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
        //�ȴ�һ��ʱ��������������ȴ���
        for (auto& it : gs_entities_)
        {
            scan_node_ids_.erase(it.second);
        }
        //����һ��ʱ���û������������id ����
        for (auto& it : scan_node_ids_)
        {
            Destroy(it);
        }
        scan_node_ids_.clear();
    }

}//namespace deploy