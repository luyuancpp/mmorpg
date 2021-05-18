/*
* https://github.com/alecthomas/entityx
 */
#ifndef COMMON_SRC_EVENT_Event
#define COMMON_SRC_EVENT_Event

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace entityx {

using Family = uint64_t;

/// Used internally by the EventManager.
class BaseEvent {
 public:
  virtual ~BaseEvent();

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
class Event : public BaseEvent {
 public:
  /// Used internally for registration.
  static Family family() {
    static Family family = family_counter_++;
    return family;
  }
};

// Functor used as an event signal callback that casts to E.
class EventCallbackWrapperBase
{
public:
    virtual ~EventCallbackWrapperBase() {}
    virtual void call(const void* event) {}
};

template <typename E>
class EventCallbackWrapper : EventCallbackWrapperBase {
public:
    explicit EventCallbackWrapper(std::function<void(const E&)> callback) : callback(callback) {}
    //void operator()(const void *event) { callback(*(static_cast<const E*>(event))); }
    virtual void call(const void* event) override { callback(*(static_cast<const E*>(event))); }
private:
    std::function<void(const E&)> callback;
};

class BaseReceiver {
 public:
  virtual ~BaseReceiver() {
    for (auto connection : connections_) {
      auto &ptr = connection.second.first;
      if (!ptr.expired()) {
        ptr.lock()->disconnect(connection.second.second);
      }
    }
  }

  // Return number of signals connected to this receiver.
  std::size_t connected_signals() const {
    std::size_t size = 0;
    for (auto connection : connections_) {
      if (!connection.second.first.expired()) {
        size++;
      }
    }
    return size;
  }

 private:
  friend class EventManager;
  std::unordered_map<Family, std::pair<EventSignalWeakPtr, std::size_t>> connections_;
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

class EventManager {
 public:
  using CallbackPtr = std::shared_ptr<EventCallbackWrapperBase>;
  using RecevierSet = std::unordered_set<BaseReceiver*>;
  using FamilyReceviers = std::unordered_map<Family, RecevierSet>;

  EventManager();
  virtual ~EventManager();

  EventManager(const EventManager&) = delete;
  EventManager& operator = (const EventManager&) = delete;

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
    auto sig = Event<E>::family();
    CallbackPtr wrapper(new EventCallbackWrapper<E>(std::bind(receive, &receiver, std::placeholders::_1)));
    auto connection = sig->connect(wrapper);
    BaseReceiver &base = receiver;
    base.connections_.insert(std::make_pair(Event<E>::family(), std::make_pair(EventSignalWeakPtr(sig), connection)));
  }

  /**
   * Unsubscribe an object in order to not receive events of type E anymore.
   *
   * Receivers must have subscribed for event E before unsubscribing from event E.
   *
   */
  template <typename E, typename Receiver>
  void unsubscribe(Receiver &receiver) {
    BaseReceiver &base = receiver;
    // Assert that it has been subscribed before
    assert(base.connections_.find(Event<E>::family()) != base.connections_.end());
    auto pair = base.connections_[Event<E>::family()];
    auto connection = pair.second;
    auto &ptr = pair.first;
    if (!ptr.expired()) {
      ptr.lock()->disconnect(connection);
    }
    base.connections_.erase(Event<E>::family());
  }

  template <typename E>
  void emit(const E &event) {
    auto sig = signal_for(Event<E>::family());
    sig->emit(&event);
  }

  /**
   * Emit an already constructed event.
   */
  template <typename E>
  void emit(std::unique_ptr<E> event) {
    auto sig = signal_for(Event<E>::family());
    sig->emit(event.get());
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
    auto sig = signal_for(std::size_t(Event<E>::family()));
    sig->emit(&event);
  }

  std::size_t connected_receivers() const {
    std::size_t size = 0;
    for (EventSignalPtr handler : handlers_) {
      if (handler) size += handler->size();
    }
    return size;
  }

 private:
  
};

}  // namespace entityx

#endif // !COMMON_SRC_EVENT_Event