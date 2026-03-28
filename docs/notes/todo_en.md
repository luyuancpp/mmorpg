# Engineering TODO / Design Notes

1. Behavior tracking v3
2. Rate-limit frequent message sending
3. Player behavior audit log — traceable operation chain per player
4. All lists must have upper bounds; warn when a threshold is exceeded
5. Warn when database fields grow too large
6. Gold exploit statistics — two rollback measures: (1) alerting system, (2) clawback system
7. When a room match is cancelled, return the original player to the front of the queue to avoid longer wait times
8. (Duplicate of #2) Rate-limit frequent message sending
9. Client data is untrusted — validate item counts for negative values
10. Auto-battle and follow logic runs server-side
11. Team system should carry extra state flags so messages don't need to be forwarded between servers
12. Base components provide only basic functionality; higher layers wrap higher-level features — layer by layer, don't pile everything into one layer
13. Only persist character data when it has actually changed
14. Framework should be simple enough that junior devs can write server code
15. Every operation should have a timestamp (for future replay/spectating support)
16. MMO spectating and replay
17. Avoid sending duplicate data
18. (Delete this todo)
19. Multi-language branching via a Unity tool for auto-switching
20. Team communication should go through message passing
21. Graceful server shutdown
22. Distributed social features: moments, friends, messaging, chat, community
23. Long background → client/server version mismatch → prompt version update on reconnect?
24. Account registration flow: wait for full download before entering the game
25. Can table IDs use uint64?
26. Test protocol packet modification
27. Production issue triage: how to quickly locate problems (e.g., IO saturation from an activity loop, or CPU saturation with no responsiveness)
28. User behavior analytics: track which features players use during testing (operation stats)
29. Error codes and user-facing tips must not be conflated
30. Movement synchronization
31. MySQL connection loss: will data queued for storage during the disconnect window be lost?
32. RPC connection interruption handling
33. Red envelope (hongbao) feature
34. Quiz / trivia feature
35. Voting feature
36. Client-side Lua hot-reload
39. Handle individual player bandwidth saturation
40. Handle server-wide bandwidth saturation
41. Single-player rollback (data restore for one player)
42. Circular reference detection in config tables
43. Server services should validate all preconditions before executing logic
44. Long disconnection while another account is already online; resume after e.g. 2 days offline
45. Character slot exhaustion during server merge
46. Evicting player data from Redis cache
47. No raw pointers in C++ objects
48. Cross-server reconnection
49. Global (all-server) leaderboard
50. Don't show "too frequent" warning after a success response
51. National memorial days: shift activity schedule by one day
52. After N reconnection attempts, require full re-login
53. Logic layering / separation of concerns
58. Force-modify player data (admin tool)
59. Clawback system for exploited resources
60. When logic is unclear, add comments first, then refine them into code
61. Mesh-topology node architecture
62. After cross-server crash, player doesn't return to origin server
64. All services should auto-recover after restart (like Honor of Kings)
65. Client-server version negotiation
66. Wrap binary-stream responses from third-party APIs into typed deserialization, so each consumer doesn't parse raw bytes
67. State machine / behavior tree / FSM patterns
68. Chat advertisement / spam filtering
69. Pseudo-hot-restart: distributed rolling update solution
70. When an error code is returned, log the full stack trace and request protocol
71. Database backup and point-in-time recovery
72. Cross-server world chat
73. Cross-server guild/clan info display
75. Low-level packet protocol validation
76. Message tampering detection
77. Integer overflow when adding negative to positive numbers
79. Accidental-operation audit log
80. Floating-point vs fixed-point arithmetic
81. Player returns online but previous scene is full
82. Client-side message filter
83. Warn if protobuf field size exceeds threshold
84. Disconnect immediately after entering the game
85. Non-tick-based timers (TimeMeter)
86. Too fine granularity → too deep call hierarchy → too many classes
87. Maintaining two copies of state for the same feature is dangerous
88. Simple code should be obviously correct at a glance
89. Player sends non-login messages without being authenticated — handle random/malicious packets
90. Character switching during cross-server play
91. Cross-server reconnect while the target server is down
92. Protobuf table field as a set type
93. Server drops the player connection
94. Accept team invitation during cross-server play
95. Warn on abnormally large per-player packet sizes
96. Chat during cross-server play
97. All rejected logic paths must return a user-facing error message (on RPC return)
98. Login queue priority: boost players with existing accounts; redirect new players to new servers
99. New activity launches often crash servers — design activities to scale horizontally
100. Low-level error logging infrastructure
101. Detect abnormal packet volume per-player over a time window
102. Traffic spike handling: activity peak load
103. Validate string length in client-to-server messages
104. Maximize decoupling between scene and player systems
105. Player has no logout timestamp if the server crashes
106. Message priority queue: under high load, drop or defer low-priority messages (that don't require ordering), then flush them to client when load subsides
107. Tiered player data (online vs offline), like the naval battles project
108. Detect oversized data in both protobuf and database fields
109. Fast issue localization: stack trace printing and logging
110. No global variables — destruction order is undefined
111. Traffic profiling (e.g., a League of Legends match uses very little bandwidth)
112. Server scenes have no "line/channel" concept — lines are a client-side display convention
113. OOP uniform abstraction vs algorithm-specific optimization: e.g., bag system treated stackable and non-stackable items identically, causing unnecessary iteration. Special-case the non-stackable path for efficiency
114. Progress reward claim status should NOT be keyed by reward ID — if a designer changes the reward ID, claim data breaks
115. 500K players crossing servers simultaneously need queue-based admission
116. Fast restart for all nodes
117. Functions should be small and compose linearly. Reusable functions should not call other reusable functions — that confuses granularity. The minimum granularity should be the unit of reuse
124. Code injection: when an open-source lib forces you to inherit or define a macro, that's injection — prefer direct-call APIs
125. Log the full request protocol and error response for every failed RPC, for easier debugging
126. Data partitioning: which data can be cleared vs not (e.g., can we wipe only event-quest data without touching main quests?)
127. How to handle in-flight player messages during scene transitions? Message loss risk
128. RPC calls between game servers (GS-to-GS)
129. (Retroactive note) Bag and team systems didn't separate base layer from logic layer
130. Feature telemetry: track which features players actually use (e.g., team follow — if nobody uses it, stop investing time)
131. Handle load/save failures gracefully
132. Alerting / monitoring system
133. Database access rate limiting / throttling
134. Sync vs async event handling
135. Config tables must store IDs only, never pointers
136. SLG timer restoration after restart
137. If templates get too complex, use code generation instead
