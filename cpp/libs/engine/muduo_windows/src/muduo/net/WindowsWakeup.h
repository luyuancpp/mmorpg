// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#ifndef MUDUO_NET_WINDOWSWAKEUP_H
#define MUDUO_NET_WINDOWSWAKEUP_H

#ifdef WIN32
namespace muduo
{
namespace net
{
namespace windows
{

// EventLoop 内部适配：沿用 int 描述符，由 EventLoop 关闭，不改变其公共接口与对象布局。
int createWakeupFd();
// 可从任意线程发送；缓冲已满时已有唤醒在排队，允许合并本次通知。
void wakeup(int wakeupFd);
// 仅在 loop 线程调用，排空水平触发的唤醒数据报。
void drainWakeupFd(int wakeupFd);

}
}
}
#endif

#endif
