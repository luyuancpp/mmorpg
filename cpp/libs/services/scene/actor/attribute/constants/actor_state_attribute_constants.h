#pragma once

enum eAttributeSyncFrequency : uint32_t {
    //kSyncEvery1Frame = 0,   // Sync every frame
    kSyncEvery2Frames = 2,  // Sync every 2 frames
    kSyncEvery5Frames = 5,  // Sync every 5 frames
    kSyncEvery10Frames = 10, // Sync every 10 frames
    kSyncEvery30Frames = 30,  // Sync every 30 frames
    kSyncEvery60Frames = 60  // Sync every 60 frames
};
