# server_updated: "Blob-ref-created" branch is dead code (vers==NULL && vers->ts_user) — inverted test

- **File:** cvsnt/cvsnt-2.5.05.3744/src/server.cpp
- **Line(s):** 4504-4510
- **Severity:** high
- **Confidence:** high
- **Category:** logic / typo

## Code
```cpp
    else if (updated == SERVER_BLOB_REF)
    {
		if ((vers == NULL && vers->ts_user == NULL) && supported_response ("Blob-ref-created"))
			buf_output0(buf_to_net,"Blob-ref-created ");
		else
			buf_output0(buf_to_net,"Blob-ref ");
	}
```
Compare the correct parallel logic for SERVER_UPDATED a few lines above (4489-4494):
```cpp
		assert (vers != NULL);
		if (vers->ts_user == NULL)
			buf_output0(buf_to_net,"Created ");
		else
			buf_output0(buf_to_net,"Update-existing ");
```

## Why this is a bug
The test `(vers == NULL && vers->ts_user == NULL)` is self-contradictory: if `vers == NULL` is true, then evaluating `vers->ts_user` dereferences a NULL pointer; if `vers` is non-NULL, the `&&` short-circuits to false. So the condition is **false in every non-crashing case** — the `"Blob-ref-created "` response is dead code and the server always emits `"Blob-ref "`.

This is the blob-reference feature that is the entire point of this Gaijin fork. The intended logic (mirroring the SERVER_UPDATED case) is "if this is a newly-created file (`vers->ts_user == NULL`) and the client understands `Blob-ref-created`, send that; otherwise send `Blob-ref`." Because the condition is inverted/broken:

- The client always receives `Blob-ref` (handled by `handle_updated_blobs_refs`, existp = `UPDATE_ENTRIES_EXISTING_OR_NEW`) instead of `Blob-ref-created` (handled by `handle_created_blobs_refs`, existp = `UPDATE_ENTRIES_NEW`).
- `UPDATE_ENTRIES_NEW` is what makes the client refuse to overwrite an untracked local file of the same name ("move away X; it is in the way"). With the create signalled as EXISTING_OR_NEW, a checkout/update that creates a blob-backed binary file can silently clobber a user's pre-existing local file of that name instead of warning.

Also, should `vers` ever legitimately be NULL on this path, the expression crashes the server outright.

## Suggested fix
```cpp
		if (vers != NULL && vers->ts_user == NULL && supported_response ("Blob-ref-created"))
			buf_output0(buf_to_net,"Blob-ref-created ");
		else
			buf_output0(buf_to_net,"Blob-ref ");
```
