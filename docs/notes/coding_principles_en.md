# Coding Principles

> These are the **philosophical / principle-level** items extracted from `todo.md` —
> they aren't "action items" (you can't check them off with code or a doc),
> but rather the judgment criteria you should continuously apply when writing code
> or making architecture decisions.
>
> That's why they've been pulled out of todo and archived separately.
>
> Source: `todo.md` items #14, #153–155, #158–162, #168–169, #172–176, #181–183, #191, #197
> Extracted: 2026-05-11

---

## I. Readability / Maintenance Cost

### 1. Keep call depth shallow — logic should be traceable at a glance
> Source: #153 #154 #160 #174 #181

**Core idea:** Fewer call chains → fewer bugs → lower comprehension cost → lower maintenance cost.

- Like indentation depth: can you tell at a glance what the code is doing?
- When a bug appears, can you spot it immediately?
- Too many call levels = technical debt = chasing bugs across multiple files, high cost to read.

**Practical heuristic:** If you need to set breakpoints in more than 3 places to track down one issue, that's a signal the logic is too tangled (see #232).

---

### 2. One-sentence summary — check that code lives in the right place
> Source: #159

**How to apply:** After writing a piece of logic, summarize it in one sentence. Then ask whether that sentence flows naturally with the surrounding code.

**Typical anti-pattern:** A block of logic that's really just "toggle UI visibility" ends up sitting inside a data-fetching interface. Write the one-sentence summary and it becomes obvious: this belongs in a "view became visible" hook (see #180).

---

### 3. Code should show what it's doing at a glance
> Source: #155 #160

Code should expose **what / flow / order** explicitly — code you can't understand probably has a bug hiding in it.

---

### 4. Simple code is where bugs hide
> Source: #88

Simple code is actually bug-prone, because nobody reads it carefully. **For anything that looks trivial, ask yourself: have all edge cases been covered?**

---

### 5. When code is obscure, don't debug it — rewrite it
> Source: #158

If a piece of code is so opaque you don't know where to start debugging, that's a sign it needs to be rewritten, not debugged.

---

## II. Debugging / Self-Review Habits

### 6. Go back and re-read your own code; use an LLM to review it
> Source: #162

Don't commit right away. Come back after half a day or a day — you'll catch things. **Run it past ChatGPT/Claude too; an LLM is a free reviewer.**

---

### 7. Step back from your current approach — try a different angle
> Source: #183

When you're stuck in an approach that isn't working, drop it and **start fresh with a completely different angle**. Grinding away at the same dead end costs more time in the long run.

---

## III. Abstraction / Refactoring

### 8. The goal of refactoring, abstraction, and polymorphism is to reduce complexity
> Source: #173

The **only** criterion for any architecture move: is it simpler or more complex after the change?

> If it looks more "sophisticated" but is harder to understand — it's a failure.

---

### 9. Only abstract when there's genuine repetition
> Source: #121 #175

**Anti-pattern:** Writing an abstraction to "demonstrate a design pattern."

**Right pattern:** You spot 3 places with genuinely duplicated code → then abstract. **Single use = don't abstract.**

---

### 10. Function granularity — reusable functions should not call other reusable functions
> Source: #117

**Keep layers clear:** The smallest reusable unit should be a leaf node only. Reusable functions calling other reusable functions makes granularity muddy.

```
┌─ Business function (chains steps in logical order)
│   ├─ Reusable fn A  ← leaf, no further nesting
│   ├─ Reusable fn B  ← leaf
│   └─ Reusable fn C  ← leaf
```

---

### 11. Logic flow should have clear steps: 1, 2, 3, 4, 5
> Source: #118

Take Unity's lifecycle as a reference (Awake / OnEnable / Start / OnDisable / OnDestroy): each step is a well-defined abstraction at a clear point in the sequence.

---

## IV. System Design Principles

### 12. A system should only change when new functionality is added — otherwise it stays put (Open/Closed Principle)
> Source: #197

Adding a feature = adding code; **don't touch existing code**. Other systems should have no effect on mine. That's the definition of "stable."

---

### 13. Decouple systems as much as possible
> Source: #172

Two systems may be related at the business level; at the code level, keep them as independent as possible. **Reuse is fine, mutual dependency is not.**

---

### 14. Don't add extension hooks inside foundational functionality
> Source: #182

**Right direction:** Extension code → calls foundational code.
**Wrong direction:** Stuffing business-specific extensions into foundational code (e.g., putting packet-send conditions inside the network layer, see #190).

---

### 15. One field, one meaning
> Source: #168

Classic anti-pattern: using `count = -1` to mean "not retrievable" — that's cramming two concepts into one field.
**Right approach:** count is a count; "is retrievable" is a separate field.
**Analogy:** A function does one thing.

---

### 16. System–developer contract: predictability
> Source: #161

If a developer uses the system by the rules, **the result should be predictable**. Predictable = testable = usable system.
A system where you don't know what will happen after a read or write is a system you can't rely on.

---

### 17. Everything needs a real use case
> Source: #175

Don't use a technology just to use it. **Start with an actual scenario, then pick the technology — not the other way around.**

---

## V. Lessons from Past Mistakes

### 18. Too many call chains and callbacks in the past
> Source: #169

Retrospective note: previous project code had call chains and callbacks nested too deep — impossible to follow the call graph or code path.

**Self-reminder:** Keep checking principle #1 (shallow call depth) as you write code.

---

### 19. Keep framework wrappers simple
> Source: #14

Goal: simple enough that **a newcomer can write server code**. Complex frameworks = slow onboarding = harder to find the right people = poor project longevity.

---

### 20. Data volume is enormous at launch
> Source: #176

**Battle-tested wisdom:** Every design decision needs to account for "launch day traffic is 100× normal."
- Will lists overflow?
- Will the database fill up?
- Will the cache get stampeded?

---

## How to use this document

1. **Before writing code:** You don't need to read through it every time, but **when you hit a design decision** (abstract or not? split into modules or not?) come back and check.
2. **During code review:** Use this as a checklist. If someone violates a principle, cite the number — no need to argue.
3. **New hire onboarding:** This is the condensed version of "why the project is written this way" — more code-level than ARCH.md.
4. **Don't keep adding new items** — unless you've genuinely found a new principle that keeps surfacing across multiple todo entries. Continuously adding will erode its value as a checklist.
