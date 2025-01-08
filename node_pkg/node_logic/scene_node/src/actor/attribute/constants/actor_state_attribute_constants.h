#pragma once

enum eAttributeSyncFrequency : uint32_t {
    //kSyncEvery1Frame = 0,   // 每帧同步一次
    kSyncEvery2Frames = 2,  // 每 2 帧同步一次
    kSyncEvery5Frames = 5,  // 每 5 帧同步一次
    kSyncEvery10Frames = 10, // 每 10 帧同步一次
    kSyncEvery30Frames = 30,  // 每 30 帧同步一次
    kSyncEvery60Frames = 60  // 每 60 帧同步一次
};
