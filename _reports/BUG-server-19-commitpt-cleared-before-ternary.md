---
id: BUG-server-19
area: commit
file: cvsnt/cvsnt-2.5.05.3744/src/rcs_checkin.cpp
line: 967
severity: low
category: logic
verdict: CONFIRMED
fix_size_loc: 3
behavior_change: yes
---

# `commitpt` is set to NULL four lines before `commitpt?'A':'M'` is evaluated, so the lock server is never told "Added"

## Summary
In `RCS_checkin`'s "empty delta tree" path, `commitpt = NULL;` is executed immediately before the `do_modified` call whose last argument is `commitpt?'A':'M'`. The ternary is therefore a compile-time constant `'M'`, and the Added/Modified distinction the call was written to convey is lost.

## Code
```cpp
// rcs_checkin.cpp:958-968
		if (commitpt != NULL && commitpt->text != NULL)
		{
			freedeltatext (commitpt->text);
			commitpt->text = NULL;
		}
		commitpt = NULL;                                            // <-- unconditionally cleared

		rcsbuf_close(&rcs->rcsbuf);
		if(atomic_checkouts)
			do_modified(lockId_temp,dtext->version,"","HEAD",commitpt?'A':'M');   // <-- always 'M'
		rcs_internal_unlockfile(fout, rcs->path, lockId_temp);
```

`commitpt` is a plain `RCSVers *` local (rcs_checkin.cpp:495) and is not reassigned between line 963 and line 967.

## Why it is a bug
`do_modified`'s `type` argument is the only thing that selects the flags word sent to the lock server:

```cpp
// lock.cpp:1290
		switch (lock_server_command(line,sizeof(line),"Modified %s|%d|%s|%s|%s\n",
			(type=='A')?"Added":(type=='D')?"Deleted":"", lockId, branch, version, oldversion))
```

and the lock server decodes it back into a transaction type:

```cpp
// lockservice/LockParse.cpp:765-769
	if(!*param)
		type='M';
	else if(!strcmp(param,"Added"))
		type='A';
	else  if(!strcmp(param,"Deleted"))
		type='D';
```

Because the ternary can only ever yield `'M'`, the `"Added"` flag is never emitted from this call site — the one place in the tree that could emit it. (The other `do_modified` call, rcs.cpp:7192 in `RCS_rewrite`, only chooses between `'D'` and `'M'`.) `grep -rn "Added" src/*.cpp` confirms `lock.cpp:1290` is the only producer.

The intent is unambiguous from context: this branch is taken when `(kf.flags&KFLAG_SINGLE) || rcs->head == NULL` (rcs_checkin.cpp:897) and prints `"initial revision: "` when `commitpt` is NULL, so the caller wanted to distinguish a brand-new file from a re-created head. Note that the surviving polarity also looks inverted — `commitpt != NULL` means a *previous* revision exists, i.e. a modification, so the intended expression was most likely `commitpt ? 'M' : 'A'`.

## Failure scenario
With `atomic_checkouts` enabled in the server configuration (main.cpp:549), commit the first revision of a new file:

```
cvs add newfile.txt
cvs commit -m "first" newfile.txt
```

`RCS_checkin` takes the `rcs->head == NULL` branch, and the server sends the lock server
`Modified |<lockId>|HEAD|1.1|` instead of `Modified Added|<lockId>|HEAD|1.1|`. The lock server stores `t.type='M'` in its `TransactionList` (LockParse.cpp:797-805) where an `'A'` was intended.

**Impact today is nil**: `grep -rn "\.type" lockservice/LockParse.cpp` shows `t.type=type;` (LockParse.cpp:803) is the only reference — the field is written and never read, so no behaviour currently depends on it. This is reported as a latent logic defect: the moment the lock server starts acting on transaction types (for atomic-checkout replay or audit), new files will be misclassified, and the bug will be invisible because the expression *looks* correct.

## Suggested fix
Capture the value before clearing, and fix the polarity while you are there:
```cpp
		char modtype = commitpt ? 'M' : 'A';

		if (commitpt != NULL && commitpt->text != NULL)
		{
			freedeltatext (commitpt->text);
			commitpt->text = NULL;
		}
		commitpt = NULL;

		rcsbuf_close(&rcs->rcsbuf);
		if(atomic_checkouts)
			do_modified(lockId_temp,dtext->version,"","HEAD",modtype);
```

## Refutation attempt
* *Is `commitpt` reassigned between 963 and 967?* No — `rcsbuf_close(&rcs->rcsbuf)` is the only intervening statement, and it takes no reference to `commitpt`.
* *Is `commitpt` a reference or macro that could alias something live?* No; `RCSVers *delta, *commitpt;` at rcs_checkin.cpp:495.
* *Could `'M'` be the correct value anyway, making this harmless?* For the `commitpt != NULL` sub-case, yes by accident. For the `commitpt == NULL` (genuinely new file) sub-case it is wrong under either reading of the intended polarity, since `'A'` is unreachable.
* *Does anything downstream depend on the type today?* No — hence low severity, stated plainly above rather than inflated.
