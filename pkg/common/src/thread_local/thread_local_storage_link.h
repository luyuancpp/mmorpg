#pragma once

#include "src/mq/rocketmqproducer.h"

class ThreadLocalStorageLink
{
public:
    ROCKETMQ_NAMESPACE::Producer producer = ROCKETMQ_NAMESPACE::Producer::newBuilder().build();
};

extern thread_local ThreadLocalStorageLink tlslink;