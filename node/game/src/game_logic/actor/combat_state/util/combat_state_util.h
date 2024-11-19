#pragma once

class SilenceAddedPbEvent;
class SilenceRemovedPbEvent;

class CombatStateUtil
{
public:
    static void AddSilence(const SilenceAddedPbEvent& event);
    
    static void RemoveSilence(const SilenceRemovedPbEvent& event);
};