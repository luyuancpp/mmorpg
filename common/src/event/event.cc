#include "event.h"


Family BaseEvent::family_counter_ = 0;

BaseReceiver::~BaseReceiver()
{
    for (auto m : managers_) {
        auto manager_ptr = m.second.first.lock();
        if (nullptr == manager_ptr)
        {
            continue;
        }
        for (auto& fmc : m.second.second)
        {
            manager_ptr->onreceiverdestroy(this, fmc.first);
        }
    }
}

void BaseReceiver::onsubscribe(const EventManagerPtr& manager_ptr, Family family_id, CallbackPtr& callback_ptr)
{
    auto manager_key = manager_ptr->manager_key();
    auto mit = managers_.find(manager_key);
    if (mit == managers_.end())
    {
        FamilyCallbacks fcs;
        EventManagerWeakPtr manager_wptr(manager_ptr);
        MangersFamilys manager_familys{ manager_wptr , fcs };
        managers_.emplace(manager_key, manager_familys);
        mit = managers_.find(manager_key);
    }
    assert(mit != managers_.end());
    auto& family_callbacks = mit->second.second;
    auto fit = family_callbacks.find(family_id);
    if (fit != family_callbacks.end())
    {
        fit->second = callback_ptr;
        return;
    }
    family_callbacks.emplace(family_id, callback_ptr);
}

void BaseReceiver::onunsubscribe(ManagerKey manager_key, Family family_id)
{
    auto mit = managers_.find(manager_key);
    if (mit == managers_.end())
    {
        return;
    }
    auto& family_callbacks = mit->second.second;
    family_callbacks.erase(family_id);
    if (family_callbacks.empty())
    {
        managers_.erase(mit);
    }
}

EventManager::~EventManager() {
    for (auto& it : family_receviers_)
    {
        for (auto& rit : it.second)
        {
            rit->onunsubscribe(this, it.first);
        }        
    }
}

