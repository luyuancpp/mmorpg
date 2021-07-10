#ifndef __QUEST_EVENT_H__
#define __QUEST_EVENT_H__

#include <array>
#include <functional>
#include <memory>


#include "StlDefineType.h"

class Obj_Human;

namespace AvatarQuest
{
    class QuestEvent
    {
    public:
        static const std::size_t kParam0 = 0;
        static const std::size_t kParam1 = 1;
        static const std::size_t kParam2 = 2;
        static const std::size_t kParam3 = 3;
        static const std::size_t kParam4 = 4;
        static const std::size_t kParamVectorSize = 5;
        using quest_event_ptr = std::shared_ptr<QuestEvent>;
        using trigger_callback_type = std::function<void(QuestEvent&)>;
        using quest_trigger_callback_type = std::function<void(int32_t, QuestEvent&)>;
        using param_vector_type = std::array<int32_t, kParamVectorSize>;

        QuestEvent(int32_t nEventType, int32_t nId, int32_t nCount)
            : m_nEventType(nEventType),
            m_nConditionId(nId),
            m_nCount(nCount)
        {
        }

        QuestEvent()
            : m_nEventType(0),
            m_nConditionId(0),
            m_nCount(1)
        {
        }

        QuestEvent(int32_t nEventType, int32_t nId, int32_t nCount, Obj_Human* pHuman)
            : m_nEventType(nEventType),
            m_nConditionId(nId),
            m_nCount(nCount),
            m_pHuman(pHuman)
        {
        }

        QuestEvent(int32_t nEventType, param_vector_type& v1, int32_t nCount)
            : m_nEventType(nEventType),
              v_param1_(v1),
              m_nCount(nCount)
        {
        }

        virtual ~QuestEvent() {}

        void Clear()
        {
            m_nEventType = 0;
            m_nConditionId = 0;
            m_nCount = 0;
        }

        int32_t GetEventType()const
        {
            return m_nEventType;
        }

        int32_t GetTargetId()const
        {
            return m_nConditionId;
        }

        int32_t GetCount()const
        {
            return m_nCount;
        }

        void LogEvent()
        {
        }

        void SetHuman(Obj_Human* pHuman) { m_pHuman = pHuman; }

        void SetEventType(int32_t nEventType) { m_nEventType = nEventType; }
        void SetTargetId(int32_t nTargetId) { m_nConditionId = nTargetId; }
        void SetCount(int32_t nCount) { m_nCount = nCount; }
        void SetQuestConfigId(int32_t quest_id) { quest_id_ = quest_id; }
        int32_t QuestConfigId()const { return quest_id_; }
        // 成就存储参数
        const param_vector_type& GetParam() const{ return v_param1_; }

    private:
        int32_t m_nEventType{ 0 };
        int32_t m_nConditionId{ 0 };
        int32_t m_nCount{ 1 };
        int32_t quest_id_{ 0 };
        Obj_Human* m_pHuman{ nullptr };
        param_vector_type v_param1_;
    };
}//namespace AvatarQuest

#endif // !__QUEST_EVENT_H__
