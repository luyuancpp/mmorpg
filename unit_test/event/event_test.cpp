#include <gtest/gtest.h>

#include <memory>
#include <iostream>
#include <string>
#include <vector>

#include "src/event/event.h"

using common::EventManager;
using common::Receiver;
using common::EventManagerPtr;

using namespace std;

struct Explosion {
    explicit Explosion(int damage) : damage(damage) {}
    int damage;
};

struct Collision {
    explicit Collision(int damage) : damage(damage) {}
    int damage;
};

struct ExplosionSystem : public Receiver<ExplosionSystem> {
    ExplosionSystem()
        : m_ptr_(EventManager::New())
    {
    }
    void receive(const Explosion& explosion) {
        damage_received += explosion.damage;
        received_count++;
    }

    void receive(const Collision& collision) {
        damage_received += collision.damage;
        received_count++;
    }

    int received_count = 0;
    int damage_received = 0;

    EventManagerPtr m_ptr_;
};

TEST(TestEmitReceive, recevie_desctory)
{
    auto ptr = EventManager::New();
    EventManager& em = *ptr.get();
    {
        ExplosionSystem explosion_system;
        em.subscribe<Explosion>(explosion_system);
        em.subscribe<Collision>(explosion_system);
        EXPECT_EQ(0, explosion_system.damage_received);
        em.emit<Explosion>(10);
        EXPECT_EQ(1, explosion_system.received_count);
        EXPECT_EQ(10, explosion_system.damage_received);
        em.emit<Collision>(10);
        EXPECT_EQ(20, explosion_system.damage_received);
        EXPECT_EQ(2, explosion_system.received_count);
    }
    EXPECT_EQ(0, em.connected_receivers());
    EXPECT_EQ(0, em.receivers<Explosion>());
    EXPECT_EQ(0, em.receivers<Collision>());
}

TEST(TestEmitReceive, unsubscribetest)
{
    EventManagerPtr ptr(EventManager::New());
    EventManager& em = *ptr.get();

    ExplosionSystem explosion_system;
    em.subscribe<Explosion>(explosion_system);
    em.subscribe<Collision>(explosion_system);
    EXPECT_EQ(0, explosion_system.damage_received);
    em.emit<Explosion>(10);
    EXPECT_EQ(1, explosion_system.received_count);
    EXPECT_EQ(10, explosion_system.damage_received);
    em.emit<Collision>(10);
    EXPECT_EQ(20, explosion_system.damage_received);
    EXPECT_EQ(2, explosion_system.received_count);

    em.unsubscribe<Explosion>(explosion_system);
    em.unsubscribe<Collision>(explosion_system);
    EXPECT_EQ(0, em.connected_receivers());
    EXPECT_EQ(0, em.receivers<Explosion>());
    EXPECT_EQ(0, em.receivers<Collision>());
    em.emit<Collision>(10);
    EXPECT_EQ(20, explosion_system.damage_received);
    EXPECT_EQ(2, explosion_system.received_count);
}

TEST(TestEmitReceive, mamgerdestroy) {
    ExplosionSystem explosion_system;
    {
        EventManagerPtr ptr(EventManager::New());
        EventManager& em = *ptr.get();
        em.subscribe<Explosion>(explosion_system);
        em.emit<Explosion>(10);
        EXPECT_EQ(10, explosion_system.damage_received);
        EXPECT_EQ(1, explosion_system.connected_signals());
        EXPECT_EQ(1, em.connected_receivers());
    }
    EXPECT_EQ(0, explosion_system.connected_signals());
}

TEST(TestEmitReceive, resubscribe) {
    ExplosionSystem explosion_system;
    EventManagerPtr ptr(EventManager::New());
    EventManager& em = *ptr.get();
    em.subscribe<Explosion>(explosion_system);
    em.subscribe<Explosion>(explosion_system);
    em.emit<Explosion>(10);
    EXPECT_EQ(10, explosion_system.damage_received);
    EXPECT_EQ(1, explosion_system.connected_signals());
    EXPECT_EQ(1, em.connected_receivers());
}


TEST(TestEmitReceive, TestEmitReceive1)
{
    auto ptr = EventManager::New();
    EventManager& em = *ptr.get();
    ExplosionSystem explosion_system;
    em.subscribe<Explosion>(explosion_system);
    em.subscribe<Collision>(explosion_system);
    EXPECT_EQ(0 , explosion_system.damage_received);
    em.emit<Explosion>(10);
    EXPECT_EQ(1 , explosion_system.received_count);
    EXPECT_EQ(10 , explosion_system.damage_received);
    em.emit<Collision>(10);
    EXPECT_EQ(20 , explosion_system.damage_received);
    EXPECT_EQ(2 , explosion_system.received_count);
}

TEST(TestEmitReceive,  TestUntypedEmitReceive)
{
    auto ptr = EventManager::New();
    EventManager& em = *ptr.get();
    ExplosionSystem explosion_system;
    em.subscribe<Explosion>(explosion_system);
    EXPECT_EQ(0 , explosion_system.damage_received);
    Explosion explosion(10);
    em.emit(explosion);
    EXPECT_EQ(1 , explosion_system.received_count);
    EXPECT_EQ(10 , explosion_system.damage_received);
}


TEST(TestEmitReceive,  TestReceiverExpired) {
    auto ptr = EventManager::New();
    EventManager& em = *ptr.get();
    {
        ExplosionSystem explosion_system;
        em.subscribe<Explosion>(explosion_system);
        em.emit<Explosion>(10);
        EXPECT_EQ(10 , explosion_system.damage_received);
        EXPECT_EQ(1 , explosion_system.connected_signals());
        EXPECT_EQ(1 , em.connected_receivers());
    }
    EXPECT_EQ(0 , em.connected_receivers());
}


TEST(TestEmitReceive, TestSenderExpired)
{
    ExplosionSystem explosion_system;
    {
        auto ptr = EventManager::New();
        EventManager& em = *ptr.get();
        em.subscribe<Explosion>(explosion_system);
        em.emit<Explosion>(10);
        EXPECT_EQ(10 , explosion_system.damage_received);
        EXPECT_EQ(1 , explosion_system.connected_signals());
        EXPECT_EQ(1 , em.connected_receivers());
    }
    EXPECT_EQ(0 , explosion_system.connected_signals());
}

TEST(TestEmitReceive,  TestUnsubscription) 
{
    ExplosionSystem explosion_system;
    {
        auto ptr = EventManager::New();
        EventManager& em = *ptr.get();
        em.subscribe<Explosion>(explosion_system);
        EXPECT_EQ(explosion_system.damage_received , 0);
        em.emit<Explosion>(1);
        EXPECT_EQ(explosion_system.damage_received , 1);
        em.unsubscribe<Explosion>(explosion_system);
        em.emit<Explosion>(1);
        EXPECT_EQ(explosion_system.damage_received , 1);
    }
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

