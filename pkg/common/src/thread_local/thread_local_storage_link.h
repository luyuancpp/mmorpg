#pragma once

#include "src/mq/rocketmqproducer.h"

class ThreadLocalStorageLink
{
public:
    std::unique_ptr<ROCKETMQ_NAMESPACE::Producer> producer;
};

extern thread_local ThreadLocalStorageLink tlslink;