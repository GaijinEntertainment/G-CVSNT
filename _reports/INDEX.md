# Report index: what is fixed and what stays open

`verdict:` in each page's frontmatter records the analysis result **against the
tree the audit examined**, before any fix. `CONFIRMED` means the defect was
verified there — it does not mean the defect is still live on this branch.
The `status:` field, where present, says what happened since.

## Fixed on this branch

Fixed in the previous slice (`audit/01-docs-and-early-fixes`):

* `BUG-blob-01` — 10.0.0.0/8 classified with a 4-bit mask
* `BUG-blob-02` — blob header split across chunks underflows
* `BUG-lib-11` — unix `recv` advances `m_bufpos` by bytes wanted

Fixed in this slice (`audit/02-analysis-reports-and-fixes`), one commit each:

* `BUG-blob-10` — zlib compress path calls `inflate` (**that call only**; the
  page's companion defects — the Unpacked-branch ctx cast and the missing
  `inflateEnd` — remain open)
* `BUG-blob-13` — `start_push_server` tests the stop pointer, not the flag
* `BUG-blob-20` — `encode_hash_str_to_blob_hash` recurses forever
* `BUG-lib-02` — `lookup_module2` fnncmp misplaced parenthesis
* `BUG-lib-04` — RootSplitter quote loop inverted; the same commit pair also
  adds the quote-reopen guard in that loop (that half has no page of its own)
* `BUG-lib-06` — repository prefix strips the wrong buffer
* `BUG-lib-17` — TokenLine escape emits ten 0x01 bytes
* `BUG-lib-24` — ServerIO syslog priority clobbered
* `BUG-lib-25` — GetOptions searches the format string for NUL
* `BUG-server-08` — write lock named with the read-lock prefix
* `BUG-server-15` — checkin reopen-failure message missing argument
* `BUG-server-17` — "pnew file" comment default (**string only**; the `pnew`
  comment lines in `import.cpp` remain open)

Two further fixes were applied and then deliberately reverted in this slice
(permission-mismatch reporting, sentinel return on error); the reasoning is in
the commit history, and their defects remain **open**.

## Everything else

`BUG-server-22` (Blob-ref-created never sent) was found during review after the
analysis pass and is open.

Every other `BUG-*` page remains open work. A cross-reference of fixes to the
regression suites (`known_issues.md`) lands with the test-suite slice
(`audit/03`).
