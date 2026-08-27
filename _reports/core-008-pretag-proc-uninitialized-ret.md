# pretag_proc returns uninitialized value when trigger library has no pretag handler

- **File:** cvsnt/cvsnt-2.5.05.3744/src/tag.cpp
- **Line(s):** 604-621
- **Severity:** medium
- **Confidence:** high
- **Category:** logic / error-handling (uninitialized variable)

## Code
```cpp
static int pretag_proc(void *params, const trigger_interface *cb)
{
	pretag_params_t *args = (pretag_params_t *)params;
	int ret;                                  // <-- not initialized

	TRACE(1,"pretag_proc(%s,%s,%s,%c)",...);

	if(cb->pretag)
	{
		pretag_list_size=pretag_list_count=0;
		walklist(tlist, pretag_list_proc, NULL);
		ret = cb->pretag(cb,args->message,args->directory,pretag_list_count,pretag_list,pretag_version_list,args->tag_type,args->action,args->tag);
		xfree(pretag_list);
		xfree(pretag_version_list);
	}

	return ret;                               // <-- garbage when cb->pretag == NULL
}
```

Compare the equivalent commit trigger in commit.cpp:1232, which correctly starts with `int ret = 0;`:
```cpp
static int precommit_proc(void *param, const trigger_interface *cb)
{
	int ret = 0;
	...
}
```

## Why this is a bug
`run_trigger` invokes `pretag_proc` for every loaded trigger/audit library. Any library that does not implement the `pretag` entry point (a perfectly normal configuration — e.g. a library providing only `postcommand` or `loginfo` hooks) makes `pretag_proc` return whatever happens to be in the stack slot of `ret`. In `check_filesdoneproc` (line 596) a positive return is treated as failure:

```cpp
	if ((n = run_trigger(&args, pretag_proc)) > 0)
    {
        error (0, 0, "Pre-tag check failed");
        err += n;
    }
```

so `cvs tag`/`cvs rtag` can nondeterministically fail with "Pre-tag check failed" (or silently succeed when it should not, if a real failure gets masked by a negative garbage value) depending on stack contents. This is exactly the class of bug that appears only in release builds and specific call paths.

## Suggested fix
```cpp
	int ret = 0;
```
