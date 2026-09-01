---
id: BUG-server-22
area: server/update core
file: cvsnt/cvsnt-2.5.05.3744/src/server.cpp
line: 4506
severity: medium
category: logic
status: open (found during review, after the analysis pass)
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `Blob-ref-created` is advertised but never sent: the test is `vers == NULL && vers->ts_user == NULL`

## Summary
`server_updated` picks the response for a blob-reference file with
`if ((vers == NULL && vers->ts_user == NULL) && supported_response ("Blob-ref-created"))`.
The first operand is a null dereference when `vers` is NULL and false otherwise, so the branch
is never taken; every `-kB` file, new or existing, goes out as plain `Blob-ref`.

## Code
```cpp
// src/server.cpp:4504-4510
    else if (updated == SERVER_BLOB_REF)
    {
		if ((vers == NULL && vers->ts_user == NULL) && supported_response ("Blob-ref-created"))
			buf_output0(buf_to_net,"Blob-ref-created ");
		else
			buf_output0(buf_to_net,"Blob-ref ");
	}
```

## Why it is a bug
The sibling branch a few lines up encodes the intended rule: `Created` when `vers->ts_user == NULL`
(no working file yet), `Update-existing` otherwise, after `assert (vers != NULL)`. The blob
branch inverted the null test, so the "file being created" distinction the client advertises
(`Valid-responses ... Blob-ref-created`, handled by `handle_created_blobs_refs`) is unreachable.

## Failure scenario
A client that supports `Blob-ref-created` checks out a `-kB` file it does not yet have. The server
sends `Blob-ref`, the update-existing form, so the client takes its existing-file path for a file
that does not exist. Today that path tolerates the absence, which is why nothing visibly breaks;
the response the protocol reserves for exactly this case is dead.

## Suggested fix
```cpp
		if (vers != NULL && vers->ts_user == NULL && supported_response ("Blob-ref-created"))
```
A wire behaviour change: clients start receiving `Blob-ref-created` for new files. Verify the
client handler with a server-mode checkout before landing.

## Refutation attempt
* Could `vers` be NULL here so the dereference matters? The `SERVER_UPDATED` branch above asserts
  `vers != NULL`, and every `SERVER_BLOB_REF` caller passes the `Version_TS` it just built, so the
  first operand is always false and the dereference never executes; the defect is the dead branch,
  not a crash.
* Does anything else send `Blob-ref-created`? `git grep` finds the literal only at this site.
