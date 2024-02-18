#pragma once

#include "src/mq/rocketmqproducer.h"

class ThreadLocalStorageLink
{
public:
    //ROCKETMQ_NAMESPACE::Producer producer;
};

extern thread_local ThreadLocalStorageLink tlslink;