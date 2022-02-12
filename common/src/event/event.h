/*
* https://github.com/alecthomas/entityx
 */
#ifndef COMMON_SRC_EVENT_EVENT_H_
#define COMMON_SRC_EVENT_EVENT_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace common 
{
using Family = uint64_t;

/// Used internally by the EventManager.
class BaseEvent 
{
 public:
     virtual ~BaseEvent() {}

 protected:
  static Family family_counter_;
};

/**
 * Event types should subclass from this.
 *
 * struct Explosion : public Event<Explosion> {
 *   Explosion(int damage) : damage(damage) {}
 *   int damage;
 * };
 */
template <typename Derived>
class Event : public BaseEvent 
{
 public:
  /// Used internally for registration.
  static Family family() {
    static Family family_id = family_counter_++;
    return family_id;
  }
};

class EventCallbackWrapperBase
{
public:
    virtual ~EventCallbackWrapperBase() {}
    virtual void call(const void* event) {}
};

template <typename E>
class EventCallbackWrapper : public EventCallbackWrapperBase 
{
public:
    explicit EventCallbackWrapper(std::function<void(const E&)> callback) : callback(callback) {}
    virtual void call(const void* event) override { callback(*(static_cast<const E*>(event))); }
private:
    std::function<void(const E&)> callback;
};

class EventManager;
using EventManagerPtr = std::shared_ptr<EventManager>;
using EventManagerWeakPtr = std::weak_ptr<EventManager>;
using CallbackPtr = std::shared_ptr<EventCallbackWrapperBase>;

class BaseReceiver {
 public:
  using FamilyCallbacks = std::unordered_map<Family, CallbackPtr>;
  using MangersFamilys = std::pair<EventManagerWeakPtr, FamilyCallbacks>;
  using ManagerKey = EventManager*;
  using Managers = std::unordered_map<ManagerKey, MangersFamilys>;

  virtual ~BaseReceiver();

  template <typename E>
  void call(ManagerKey manager_key, Family family_id, const E& e)
  {
      auto mit = managers_.find(manager_key);
      if (mit == managers_.end())
      {
          return;
      }
      auto& manager_familys = mit->second.second;
      auto fit = manager_familys.find(family_id);
      if (fit == manager_familys.end())
      {
          return;
      }
      fit->second->call(&e);
  }

  // Return number of signals connected to this receiver.
  std::size_t connected_signals() const {
    std::size_t size = 0;
    for (auto m : managers_) {
       size += m.second.second.size();
    }
    return size;
  }

 private:
     void onsubscribe(const EventManagerPtr& manager_ptr, Family family_id, CallbackPtr& callback_ptr);
     void onunsubscribe(ManagerKey manager_key, Family family_id);

  friend class EventManager;
  Managers managers_;
};

template <typename Derived>
class Receiver : public BaseReceiver {
 public:
  virtual ~Receiver() {}
};

/**
 * Handles event subscription and delivery.
 *
 * Subscriptions are automatically removed when receivers are destroyed..
 */

class EventManager : public std::enable_shared_from_this<EventManager> {
 public:
  using BaseReceiverPtr = BaseReceiver*;
  using Receviers = std::unordered_set<BaseReceiverPtr>;
  using FamilyReceviers = std::unordered_map<Family, Receviers>;

  EventManager() {}
  ~EventManager();

  static EventManagerPtr New() { return std::make_shared<EventManager>(); }

  EventManager(const EventManager&) = delete;
  EventManager& operator = (const EventManager&) = delete;

  EventManager* manager_key(){ return this; }

  std::size_t connected_receivers() const {
      std::size_t size = 0;
      for (auto& it : family_receviers_)
      {
          size += it.second.size();
      }
      return size;
  }

  template <typename E>
  std::size_t receivers()
  {
      std::size_t size = 0;
      auto family_id = Event<E>::family();
      auto it = family_receviers_.find(family_id);
      if (it == family_receviers_.end())
      {
          return 0;
      }
      return family_receviers_.size();
  }

  /**
   * Subscribe an object to receive events of type E.
   *
   * Receivers must be subclasses of Receiver and must implement a receive() method accepting the given event type.
   *
   * eg.
   *
   *     struct ExplosionReceiver : public Receiver<ExplosionReceiver> {
   *       void receive(const Explosion &explosion) {
   *       }
   *     };
   *
   *     ExplosionReceiver receiver;
   *     em.subscribe<Explosion>(receiver);
   */
  template <typename E, typename Receiver>
  void subscribe(Receiver &receiver) {
    void (Receiver::*receive)(const E &) = &Receiver::receive;
    auto family_id = Event<E>::family();
    CallbackPtr wrapper(new EventCallbackWrapper<E>(std::bind(receive, &receiver, std::placeholders::_1)));
    auto it = family_receviers_.find(family_id);
    if (it == family_receviers_.end())
    {
        Receviers receviers{ &receiver };
        family_receviers_.emplace(family_id, receviers);
        it = family_receviers_.find(family_id);
    }
    assert(it != family_receviers_.end());
    auto rit = it->second.find(&receiver);
    if (rit == it->second.end())
    {
        it->second.emplace(&receiver);
        rit = it->second.find(&receiver);
    }
    assert(rit != it->second.end());
    (*rit)->onsubscribe(shared_from_this(), family_id, wrapper);
  }

  /**
   * Unsubscribe an object in order to not receive events of type E anymore.
   *
   * Receivers must have subscribed for event E before unsubscribing from event E.
   *
   */
  template <typename E, typename Receiver>
  void unsubscribe(Receiver &receiver) 
  {
    auto family_id = Event<E>::family();
    auto it = family_receviers_.find(family_id);
    if (it == family_receviers_.end())
    {
        return;
    }
    auto& receivers = it->second;
    auto rit = receivers.find(&receiver);
    if (rit == receivers.end())
    {
        return;
    }
    (*rit)->onunsubscribe(this, family_id);
    receivers.erase(rit);
    if (receivers.empty())
    {
        family_receviers_.erase(it);
    }
  }

  template <typename E>
  void emit(const E &event) {
    auto family_id = Event<E>::family();
    auto it = family_receviers_.find(family_id);
    if (it == family_receviers_.end())
    {
        return;
    }
    for (auto& receiver : it->second)
    {
        receiver->call(this, family_id, event);
    }
  }

  /**
   * Emit an already constructed event.
   */
  template <typename E>
  void emit(std::unique_ptr<E> event) {
    auto family_id = Event<E>::family();
    emit(event.get());
  }

  /**
   * Emit an event to receivers.
   *
   * This method constructs a new event object of type E with the provided arguments, then delivers it to all receivers.
   *
   * eg.
   *
   * std::shared_ptr<EventManager> em = new EventManager();
   * em->emit<Explosion>(10);
   *
   */
  template <typename E, typename ... Args>
  void emit(Args && ... args) {
    // Using 'E event(std::forward...)' causes VS to fail with an internal error. Hack around it.
    E event = E(std::forward<Args>(args) ...);
    emit(event);
  }

  void onreceiverdestroy(BaseReceiverPtr receiver, Family family_id)
  {
      auto it = family_receviers_.find(family_id);
      if (it == family_receviers_.end())
      {
          return;
      }
      auto& receivers = it->second;
      auto rit = receivers.find(receiver);
      if (rit == receivers.end())
      {
          return;
      }
      receivers.erase(rit);
      if (receivers.empty())
      {
          family_receviers_.erase(it);
      }
  }
 private:     
     FamilyReceviers family_receviers_;
};

}  // namespace common

#endif // COMMON_SRC_EVENT_EVENT_H_
