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
118. Logic steps should be numbered (step 1, step 2, …), like Unity's Awake/OnEnable/OnDisable lifecycle
119. Priority message queue
120. System priority levels
121. Only abstract/encapsulate when actual code duplication exists, or it's a base component
122. Optimize hot paths — focus on performance bottlenecks
123. Server multi-threading via message queues for high performance, not locks
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
138. If two modules within the same feature can be independent, separate them. E.g., loot presentation vs loot state — they look related but can be fully decoupled
139. Timer reload (e.g., during a memorial period, shift all timers forward)
140. Don't share the same table field across different systems
141. Some logic operations should be atomic. E.g., auction: current bid / my bid / buy-now must all change in one atomic block. If any step fails, no data should change
142. Teleport failure into an instance must not leave the player in a void state
143. Some things should be tools, not code
144. Static data ↔ dynamic data conversion patterns
145. Config tables should avoid multi-value columns
146. Base functionality vs logic reuse: extract reusable logic into utility functions that call base components, don't embed it in the base layer
147. In-memory data migration between processes (data not persisted to DB)
148. Only the owning entity should mutate its own state. Don't let one player's code directly modify another player's state
149. Graceful server shutdown (expanded)
150. Instance god-mode: observe other players' state
151. NPC combat detail / instance combat replay detail
152. Distributed tracing for operation tracking (UI + backend)
153. Fewer call chains = fewer bugs = faster comprehension. Like indentation depth: can you see what the code does at a glance? Can you spot a bug immediately? This is a maintenance cost factor
154. Shallow call hierarchies → faster logic tracing and bug hunting
155. Code should be readable at a glance
156. Gate async concurrent login case
157. Gate async concurrent login: if old gate's logout arrives at centre after new gate's login, does login fail? How to handle?
158. How to debug obscure code paths
159. Summarize a code block in one sentence, then check if it matches surrounding logic. If not, it might be misplaced. E.g., if logic only controls show/hide, wire it from the "view becomes visible" hook, not from arbitrary return paths
160. Code should reveal: what it does, the full flow, the execution order — so problems are easy to find. Unreadable code probably has bugs
161. System-developer contract: if devs follow the rules, read/write results are predictable. Predictability ensures determinism. An unpredictable system is hard to use
162. Review your own code repeatedly — you'll find issues. Use ChatGPT to assist
163. Server should push data to client, not wait for client to request it — client doesn't know when server finishes scene transition
164. C++ devs should minimize macro usage — preprocessor just does text substitution with no type checking, and macro bugs are hard to trace
165. Auction review: if all clients request at 10:01 AM, instantaneous load spike. Plan for burst traffic at fixed times
166. Dedicated network thread for receiving messages: allows scene tick to continue while network I/O proceeds, preventing kernel buffer saturation
167. Allow players to change their login region/server
168. One field = one meaning. Don't overload a field with different semantics in different contexts (e.g., using reward-recovery-count = -1 to mean "non-recoverable"). Use separate fields for separate concepts, like "one function does one thing"
169. (Retroactive note) Previous code had too many call chains and callbacks — couldn't see the flow at a glance
170. GUID recycling: moving an item = delete + add → generates new GUID each time. Better to swap
171. Hot-reload support for new config table rows
172. Decouple systems as much as possible — even if they look related, make them independent at the code level. Reuse is still fine
173. Every refactoring / abstraction / polymorphism should reduce complexity
174. (Duplicate of #154) Shallow call hierarchies → faster debugging
175. Apply techniques only when there's a real use case — don't use patterns just to use them
176. Handle large data volumes at launch time
178. Prerequisites / precondition system
179. Feature unlock / condition system
180. Centralized refresh hook: e.g., always refresh UI data when a view becomes visible (not just on "back" navigation). Timer-based refresh is bad (IO saturation + stale data)
181. Tech debt: deep call chains → hard to trace bugs → more complexity → more bugs
182. Base features must not include extension logic; extensions should call base features
183. When stuck in current logic, step back and try a completely different approach
184. 2D state-condition table
185. 2D condition-feature validation table
186. Classes must not have pointer member variables
187. Any extracted byte stream should be directly deserializable into the target type via template
188. Compare two approaches before choosing. E.g., for iterator-delete: copy-and-iterate vs collect-then-delete — the latter is better (smaller working set)
187b. UE-style relevancy sync: closer objects get more frequent updates. Extend to login queue: higher position = faster status refresh rate. UE also varies sync rate by object type and importance
188b. Client-server interaction: prefer pull over push. Let client send requests and control timing, adapting to its runtime environment. This gives client flexibility to fetch only what it needs
189. CPU spike / jitter poses latent performance risk
190. Low-level systems should not contain business logic (e.g., don't put send-packet conditions in the network layer)
191. Code layering discipline
192. Protobuf field length validation
193. Players on different game nodes sending messages to each other
194. Delete a specific database row
195. Handle individual player network anomalies
196. Pause functionality in games
197. A system should only change when new features are added; otherwise it stays stable. Other systems should never affect mine
198. MMO camera switch to teammate / spectate feature
199. Operating on a player's offline data
200. Data sync after service restart
201. Spectating system
202. Spectate time compression / pause technology
203. "Item already in use" prompt when using an item before previous response arrives
204. Dirty-flag storage: only persist when data has actually changed
205. Validate message list lengths
206. Validate client-sent data sizes
207. Tentative currency deduction (cross-server auth required for final confirmation)
208. Player on one game-scene server sending message directly to a player on another game-scene server
209. Golang login authentication
210. Importing external network data into internal network (security boundary)
211. Copying single-player data into internal network
212. Data sync for various server restart scenarios
213. Graceful server shutdown (final)
214. Login queue using open-source rate limiter
215. Quest type fields must be distinct: guild quests get their own type enum, not shared with daily quest enum. Daily quests should be handled by the daily quest system, not the base quest system
216. Send a signal to server to dump all thread stack traces
217. Player career: historical operation records and events
218. DB layer should have no business logic — it should only care about storage. (Previous mistake: DB service was creating characters)
~~219. Stateless microservice readiness gate: register with service discovery only after initialization completes, with readiness probes and watch mechanism to prevent traffic to unready instances~~ ✅ Done: DependencyGate abstraction in node.h, Scene/Gate main.cpp integrated