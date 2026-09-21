---
id: BUG-build-02
area: build / Windows project files
file: cvsnt/cvsnt-2.5.05.3744/cvsnt.vcxproj
line: 188
severity: medium
category: correctness
verdict: CONFIRMED
fix_size_loc: 1
behavior_change: yes
---

# `Release|x64` compiles `cvs.exe` with C++ exceptions disabled, but `setuid.cpp` contains a live `try`/`catch` — the 64-bit release binary has no unwind semantics where the 32-bit one does

## Summary
`<ExceptionHandling>false</ExceptionHandling>` appears in exactly one of the four configurations of
`cvsnt.vcxproj` — `Release|x64`. That configuration compiles
`windows-NT/setuid.cpp`, which wraps its Active Directory name-translation in a
`try`/`catch(_com_error)`. The block relies on `_com_ptr_t` throwing `_com_error` on failure, which
is the only way that COM smart pointer reports an error. Compiled without an exception-handling
model, MSVC emits no unwind code for that frame (warning C4530), so the shipped 64-bit binary and
the 32-bit binary differ in how they behave when the AD query fails.

## Code
```xml
<!-- cvsnt.vcxproj:157 -->
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
  <ClCompile>
    ...
    <!-- cvsnt.vcxproj:188 -->
    <ExceptionHandling>false</ExceptionHandling>
  </ClCompile>
```

`Release|Win32` (`cvsnt.vcxproj:108`), `Debug|Win32` and `Debug|x64` do **not** set it — this is the
only occurrence of the element in the file.

The code it compiles:
```cpp
// windows-NT/setuid.cpp:501-518
	try
	{
		ActiveDs::IADsNameTranslatePtr info(CLSID_NameTranslate);

		wsprintfW(Buf.Upn,L"%s\\%s",wszDomain,wszUser);
		TRACE(3,"S4U untranslated name: %S",Buf.Upn);

		info->Init(ADS_NAME_INITTYPE_GC,L"");
		info->Set(ADS_NAME_TYPE_NT4,Buf.Upn);
		lstrcpyW(Buf.Upn,info->Get(ADS_NAME_TYPE_USER_PRINCIPAL_NAME));

		TRACE(3,"S4U UPN: %S",Buf.Upn);
	}
	catch(_com_error e)
	{
		TRACE(3,"IADS query failed: %S",e.ErrorMessage());
		return ERROR_INVALID_FUNCTION;
	}
```

`windows-NT/setuid.cpp` is compiled into this project (`cvsnt.vcxproj:691`), and it is the **only**
file among `src/*.cpp` and `windows-NT/*.cpp` that contains a `catch(` — so this single block is the
entire exception surface of the binary.

## Why it is a bug
With no `/EH` flag, MSVC does not emit unwind tables for the function. Two consequences:

1. `ActiveDs::IADsNameTranslatePtr info` is an automatic object with a destructor that calls
   `Release()` on the COM interface. When `info->Init()` or `info->Set()` throws, that destructor is
   not run, so the interface is leaked for the life of the process.
2. More broadly, the compiler is entitled to assume no exception propagates through the frame. The
   observable behaviour of a throw here is not defined by the language settings in force, and it
   differs between the two release configurations of the same product.

The divergence is what makes this worth fixing regardless of how the runtime happens to behave
today: a failure mode reproduced on a 32-bit build will not necessarily reproduce on the 64-bit
build that users actually run, and vice versa.

This is also the reason a from-source build of this configuration emits C4530
("C++ exception handler used, but unwind semantics are not enabled") for `setuid.cpp`.

## Failure scenario
Run the 64-bit release `cvs.exe` as a server on a Windows host joined to a domain, with S4U
impersonation in use. Point it at a user whose account cannot be resolved by the global catalog —
a stale account, a trust that is down, or a domain controller that is unreachable.
`IADsNameTranslate::Set` fails, `_com_ptr_t` throws `_com_error`, and the frame unwinds without
running `~IADsNameTranslatePtr`. Each such logon attempt leaks one COM interface reference; a server
process handling repeated failed logons accumulates them for its lifetime. The equivalent 32-bit
build releases the interface correctly, so the leak is invisible to anyone testing on Win32.

## Suggested fix
Remove the element so `Release|x64` matches the other three configurations:

```xml
<!-- cvsnt.vcxproj:188 — delete this line -->
<ExceptionHandling>false</ExceptionHandling>
```

Alternatively, set it to `Sync` (`/EHsc`) explicitly in all four configurations, which states the
intent rather than relying on the default. Do not instead remove the `try`/`catch` — it is the only
error path `_com_ptr_t` offers.

## Refutation attempt
Checked whether `setuid.cpp` might be excluded from the x64 build — it is not; `cvsnt.vcxproj:691`
lists it unconditionally. Checked whether some other configuration also disables exceptions, which
would make this a deliberate global policy rather than an oversight — `grep -n ExceptionHandling`
returns exactly one hit in the file, so `Release|x64` is the odd one out. Checked whether any other
compiled source in the project uses `try`/`catch`, which would make the setting obviously untenable
and therefore likely already known — only `setuid.cpp` does, which explains how this survived.
Checked whether `_com_ptr_t` can be configured not to throw — it can (`_COM_SMARTPTR_TYPEDEF` with
`_com_ptr_t` raw methods), but this code uses the throwing form (`info->Init(...)`, not
`info->raw_Init(...)`), so the `catch` is load-bearing. The finding stands.
