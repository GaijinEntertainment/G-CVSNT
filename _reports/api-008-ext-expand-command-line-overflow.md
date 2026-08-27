---
# ext expand_command_line: unbounded %-expansion strcpy and off-by-one terminator write
- **File:** cvsnt/cvsnt-2.5.05.3744/protocols/ext.cpp
- **Line(s):** 219-249
- **Severity:** low
- **Confidence:** high
- **Category:** overflow

## Code
```cpp
int expand_command_line(char *result, int length, const char *command, const cvsroot* root)
{
    const char *p;
    char *q;

    q=result;
    for(p=command; *p && (q-result)<length; p++)
    {
        if(*p=='%')
        {
            switch(*(p+1))
            {
            ...
            case 'u':
                strcpy(q, get_username(root));   // no bound check
                q+=strlen(q);
                break;
            case 'h':
                strcpy(q, root->hostname);       // no bound check
                q+=strlen(q);
                break;
            case 'd':
                strcpy(q, root->directory);      // no bound check
                q+=strlen(q);
                break;
            }
            p++;
        }
        else
            *(q++)=*p;
    }
    *(q++)='\0';                                 // can write result[length]
    return 0;
}
```

## Why this is a bug
Two distinct overruns of the caller's `result` buffer (`command_line[1024]` in
`ext_connect`):

1. The loop guard `(q-result)<length` limits the number of *iterations* but not the
   length written per iteration. Each `%u`/`%h`/`%d` case does a raw `strcpy` of the
   username / hostname / directory into `q` with no remaining-space check, so a long
   CVSROOT field writes past the end of `result`.

2. Off-by-one on the terminator: the char-copy branch can advance `q` until
   `q-result == length` (it writes `result[length-1]` and increments), the loop then
   exits, and `*(q++)='\0'` writes `result[length]` — one byte beyond the buffer — even
   when no `%` expansion is involved.

These fields originate from the (client-side) CVSROOT, so this is not typically
remotely controlled, but overlong values (a long repository `directory`, a long FQDN)
corrupt the stack. The command string is then handed to `run_command`.

## Suggested fix
Track remaining space and bound every write, e.g. build with `snprintf`/`cvs::sprintf`
into a sized buffer, or before each `strcpy` verify
`strlen(src) < length-(q-result)` and truncate/fail otherwise; reserve one byte for the
final NUL so the loop stops at `length-1`.
---
