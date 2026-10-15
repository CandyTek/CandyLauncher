# 32-bit vs 64-bit Release: Memory and Speed

Measured on 2026-10-15 to answer: "If we only ship a 32-bit build, do we save a lot of memory, and does it stay as fast?"

**Short answer:** no. The 32-bit build saves about 4 MB (around 8%), runs at the same speed, and loses features. Keep x64 as the primary release.

## Setup

- Trees: `cmake-build-release-ninja-vs` (x64) and `cmake-build-release-ninja-vs-x86` (x86, generated with `VsDevCmd.bat -arch=x86`), same source, both Release.
- Same `user_settings.json` in both: 19 plugins enabled, fuzzy match on, about 775 indexed items.
- Each build was launched, left for 12 s to finish indexing, then driven with 68 search queries x 5 rounds sent to the hidden edit control via `WM_SETTEXT`. Filtering runs synchronously inside that call, so the round-trip time is the real per-keystroke cost.
- Memory was read with `GetProcessMemoryInfo` (private usage and working set). Each build was measured twice; the tables show the median.

## Memory

| Build | Private bytes | Working set | Private after 340 queries |
|-------|---------------|-------------|---------------------------|
| x64   | 54 MB         | 73.5 MB     | 59 MB                     |
| x86   | 50 MB         | 66.5 MB     | 56 MB                     |

About 1.2 MB of the 4.5 MB gap is the 42 Control Panel items (plus icons) that the x86 build never indexes, so the real pointer-width saving is roughly 3 MB.

## Speed

| Build | Median per query | CPU for the whole query burst |
|-------|------------------|-------------------------------|
| x64   | 1.07 - 1.33 ms   | 1.9 - 2.3 s                   |
| x86   | 1.11 - 1.15 ms   | 1.8 - 2.0 s                   |

No measurable difference. Index timings (Everything, bookmarks, browser history, Windows Settings) were within noise on both.

## What the 32-bit build loses

- **Control Panel items disappear.** A WOW64 process cannot enumerate the Control Panel shell namespace; the log shows `index source=control_panel count=0` on x86 versus 42 items on x64. `CandyUwpHelper.exe` is also 32-bit in that build, so it does not help.
- **64-bit shell extensions do not load** in a 32-bit process, so the right-click menu on indexed files loses entries from 7-Zip, Git clients and similar tools.
- **System32 is redirected to SysWOW64**, so `%PATH%` indexing and anything touching System32 sees the 32-bit file set.

## Conclusion

Ship x64 as the main release. Offer x86 only as a legacy download for 32-bit Windows. Memory work should target individual plugins (see the per-plugin footprint notes), not the architecture.
