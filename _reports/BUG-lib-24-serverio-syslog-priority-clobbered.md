---
id: BUG-lib-24
area: cvsapi/cvstools/lib
file: cvsnt/cvsnt-2.5.05.3744/cvsapi/ServerIO.cpp
line: 159
severity: medium
category: logic
status: fixed in this slice (audit/02)
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `syslog(l | LOG_NOTICE, …)` re-ORs a priority into an already-complete one, downgrading every `logError` message from LOG_ERR to LOG_DEBUG

## Summary
`CServerIo::log()` builds a complete syslog value in `l` (facility OR priority) in the switch
above, then passes `l | LOG_NOTICE` to `syslog()`. Syslog priorities are small integers packed
into the low three bits, not flags, so OR-ing `LOG_NOTICE` (5) into `LOG_ERR` (3) yields 7 —
`LOG_DEBUG`. Every error the server logs to syslog is therefore emitted at debug priority, which
most syslog configurations discard.

## Code
```cpp
// cvsapi/ServerIO.cpp:136-160
	int l;

	switch(type)
	{
	case logNotice:
		l = LOG_DAEMON | LOG_NOTICE;
		break;
	case logError:
		l = LOG_DAEMON | LOG_ERR;
		break;
	case logAuth:
		l = LOG_AUTHPRIV | LOG_NOTICE;
		break;
	default:
		l = LOG_DAEMON | LOG_NOTICE;
		break;
	}
	syslog(l | LOG_NOTICE, "%s", str.c_str());      // <-- line 159
```

## Why it is a bug
`<syslog.h>` defines priorities as consecutive integers in the low three bits and facilities as
values shifted left by 3:

```
LOG_EMERG 0, LOG_ALERT 1, LOG_CRIT 2, LOG_ERR 3, LOG_WARNING 4, LOG_NOTICE 5, LOG_INFO 6, LOG_DEBUG 7
LOG_DAEMON (3<<3) == 24     LOG_AUTHPRIV (10<<3) == 80
```

They are extracted with `LOG_PRI(p) ((p) & LOG_PRIMASK)` where `LOG_PRIMASK` is `0x07` — a mask,
not a flag set. OR-ing two priorities produces a third, unrelated one:

| `type` | `l` | `l \| LOG_NOTICE` | priority actually logged |
|---|---|---|---|
| `logNotice` | 24\|5 = 29 | 29 | LOG_NOTICE — correct by accident |
| `logAuth`   | 80\|5 = 85 | 85 | LOG_NOTICE — correct by accident |
| default     | 24\|5 = 29 | 29 | LOG_NOTICE — correct by accident |
| **`logError`** | 24\|3 = **27** | 24\|(3\|5) = 24\|**7** = **31** | **LOG_DEBUG** |

The `| LOG_NOTICE` is redundant for three of the four cases, which is why it went unnoticed — it
only changes anything in the one case that matters.

## Failure scenario
`CServerIo::log(CServerIo::logError, …)` is the fatal-error channel:

* `cvstools/ProtocolLibrary.cpp:51` —
  `CServerIo::log(fatal?CServerIo::logError:CServerIo::logNotice,"%s",text);`, the sink for every
  protocol-plugin fatal error;
* `lockservice/server.cpp:277, 296, 353, 363` — "Failed to create listening socket",
  "Failed to bind listening socket", "Failed to create UD socket", "Failed to bind UD socket";
* `lockservice/lockservice.cpp:257, 275` — service start-up failures.

A typical `/etc/rsyslog.conf` ships with `*.info;mail.none;authpriv.none;cron.none
/var/log/messages` — `daemon.debug` is below `info` and is dropped. So when the CVSNT lock
service fails to bind its listening socket, the operator sees **nothing** in
`/var/log/messages`; a `journalctl -p err` sweep finds nothing either. Meanwhile the routine
notices (service started/stopped) do get through, so the log looks healthy while the errors are
invisible.

## Suggested fix
```cpp
	syslog(l, "%s", str.c_str());
```

## Refutation attempt
- Verified from `<sys/syslog.h>` that `LOG_PRIMASK` is `0x07` and priorities are extracted with a
  mask, so `LOG_ERR | LOG_NOTICE` is `7` (`LOG_DEBUG`) and not "either of the two".
- Checked whether some implementation might treat the priority as a bitmask where OR-ing is
  meaningful: it does not — `setlogmask()` uses `LOG_MASK(pri)` precisely because the priority
  itself is an ordinal.
- Checked the other three switch arms to be sure the redundant OR is genuinely a no-op there (it
  is: `5 | 5 == 5`), confirming this is a stale leftover rather than a deliberate "at least
  notice" floor — a floor would have to be `MIN`, not `OR`, since lower numbers are *more* severe.
- Checked that the `#else` branch really is the live one on unix: the `#ifdef _WIN32` path above
  (line 139-140) uses `ReportError()`/the Windows event log and is unaffected.
- Noted in passing (outside this audit's scope): `cvsservice/Service.cpp:768` calls
  `CServerIo::log(..., buf)` passing a runtime string as the `fmt` parameter — a format-string
  bug. `cvstools/ProtocolLibrary.cpp:51` gets this right with `"%s",text`.
