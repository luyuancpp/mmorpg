# AFK (挂机) Detection Design

## Problem
MMO games need to detect whether a player is AFK (away from keyboard) or using scripts to auto-play.

## Two-Layer Approach

### Layer 1: Message-based timeout (Gate side, recommended first step)
- Gate is the entry point for all client messages.
- Maintain `last_active_time` per player session.
- Use **reverse exclusion**: exclude a small set of automatic messages (heartbeat, client status report), treat all others as active input.
- Periodically check: `now() - last_active_time > AFK_THRESHOLD` -> mark player as AFK, notify Scene.
- Pseudo-logic:
  ```
  OnClientMessage(player, msg_id):
      if msg_id not in {HEARTBEAT, CLIENT_STATUS_REPORT}:
          player.last_active_time = now()
  ```
- **Pros**: simple, low overhead, no business logic intrusion.
- **Cons**: scripts can spoof active messages (e.g., send periodic move packets).

### Layer 2: Behavioral pattern analysis (Scene side, optional advanced)
- Detect repetitive movement paths (fixed-route patrol).
- Detect overly regular operation intervals (machine-like precision).
- Detect fixed skill rotation sequences.
- **Pros**: catches scripted AFK.
- **Cons**: complex, risk of false positives.

## Implementation Notes
- Layer 1 is sufficient for detecting genuine AFK (player left keyboard).
- Layer 2 is needed only if anti-bot/anti-script enforcement is required.
- AFK threshold is configurable (e.g., 5 minutes default).
- AFK status change should be broadcast to Scene so gameplay systems can react (e.g., reduce rewards, kick from dungeon).

## Status
- Design recorded, not yet implemented.
