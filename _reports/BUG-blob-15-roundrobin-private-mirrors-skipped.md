---
id: BUG-blob-15
area: blob/CAFS subsystem
file: cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
line: 207
severity: medium
category: logic
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# Mirror round-robin compares `attempt < privateCount` instead of `attempt < publicCount + privateCount`, so most private mirrors are never tried

## Summary
`RoundRobin::shuffle()` maps attempt numbers onto `[public mirrors][private mirrors][master]`.
The private-mirror test forgets to add `publicCount` to the bound, so the private window is
`[publicCount, privateCount)` instead of `[publicCount, publicCount + privateCount)`. Whenever
`publicCount >= privateCount` the window is empty and *no* private mirror is ever used; otherwise
only `privateCount - publicCount` of them are.

## Code
```cpp
// src/download_blob_to.cpp:197-210
uint32_t BackgroundProcessor::RoundRobin::shuffle(uint32_t attempt, uint32_t id) const
{
  if (urls.empty())
    return 0;
  const uint32_t urlsCnt = uint32_t(urls.size());
  if (publicCount <= 1 && privateCount <= 1)//last one is Master, and nothing to shuffle
    return attempt%urlsCnt;
  const uint32_t shuffledId = id + shuffleStart;
  if (attempt < publicCount)//first round robin on all public addresses
    return (attempt + shuffledId)%publicCount;
  if (attempt < privateCount)//then round robin on all private addresses    <-- BUG
    return publicCount + uint32_t(attempt-publicCount + shuffledId)%privateCount;
  return urlsCnt-1;//master
}
```

## Why it is a bug
`init()` builds `urls` as public entries first, then private entries, then the master last
(`:261-283`), and `attemptsCount()` returns `urls.size()` == `publicCount + privateCount + 1`
(`:167-170`), so callers walk `attempt` from 0 to `publicCount + privateCount`.

The first branch already establishes the convention: attempts `[0, publicCount)` index the public
block. The second branch must therefore cover attempts `[publicCount, publicCount + privateCount)`.
Writing `attempt < privateCount` compares an absolute attempt index against a *count of a later
block*. The index computation on the same line already subtracts the offset
(`attempt - publicCount`), which confirms the intended bound.

Worked examples (`urlsCnt = publicCount + privateCount + 1`):

| publicCount | privateCount | attempts hitting private | should be |
|---|---|---|---|
| 3 | 2 | none (`3 < 2` false) | 3, 4 |
| 2 | 3 | only attempt 2 | 2, 3, 4 |
| 1 | 5 | attempts 1..4 | 1..5 |

The `fail()` diagnostic is derived from `attempt` against `urls.size()` (`:179-183`), so it prints
"Switching to next"/"Switching to master" for attempts that in reality all resolve to the master —
the operator-facing message does not match what happens.

## Failure scenario
A studio configures three public CAFS mirrors and two private (office-LAN) mirrors, plus the master.
`publicCount = 3`, `privateCount = 2`, `urls.size() = 6`.

A developer in the office runs `cvs update`. `KVNetworkProcessor::init()`
(`blob_kv_processor.cpp:101-106`) walks `attempt` 0..5:

* attempts 0,1,2 -> `shuffle` returns 0..2 -> the three public mirrors (WAN round trip);
* attempt 3 -> `3 < 2` is false -> `urlsCnt-1 = 5` -> **master**;
* attempt 4 -> master; attempt 5 -> master.

The two fast local mirrors at indices 3 and 4 are dead configuration. Every developer whose nearest
public mirror is down falls straight through to the master, which is exactly the load the private
mirrors exist to absorb. Because `fail()` also marks only the URLs that `shuffle` returns, the
private mirrors are never even health-checked.

## Suggested fix
```cpp
  if (attempt < publicCount + privateCount)//then round robin on all private addresses
    return publicCount + uint32_t(attempt-publicCount + shuffledId)%privateCount;
```

## Refutation attempt
I checked whether `publicCount`/`privateCount` might already be cumulative (which would make the
comparison correct): `createShuffles(clientsCount, publicUrlsCnt, privateUrlsCnt)` (`:286`) is
called with two independent counters incremented in two separate loops (`:267`, `:277`), so they are
plain counts, and `urls.size() == publicUrlsCnt + privateUrlsCnt + 1`. I checked whether the
returned index could go out of range — it cannot; `publicCount + (...)%privateCount` is at most
`urlsCnt - 2` — so this is a reachability bug, not a memory-safety one. I also checked the
`publicCount <= 1 && privateCount <= 1` fast path, which correctly cycles all URLs and is why small
configurations behave sensibly and hide the bug. The finding stands.
