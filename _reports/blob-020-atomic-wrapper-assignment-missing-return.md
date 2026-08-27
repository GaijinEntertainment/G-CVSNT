# atomic_wrapper copy-assignment operator has no return statement (UB)

- **File:** cvsnt/cvsnt-2.5.05.3744/src/download_blob_to.cpp
- **Line(s):** 48
- **Severity:** low
- **Confidence:** high
- **Category:** logic

## Code
```cpp
template <typename T>
struct atomic_wrapper
{
  std::atomic<T> _a;
  atomic_wrapper():_a(){}
  atomic_wrapper(const std::atomic<T> &a):_a(a.load()){}
  atomic_wrapper(const atomic_wrapper &other):_a(other._a.load()){}
  atomic_wrapper &operator=(const atomic_wrapper &other) {_a.store(other._a.load());}  // <-- no return
  operator T() {return T(_a);}
  atomic_wrapper(const T& other) { _a = other; }
  atomic_wrapper& operator=(const T& other) { _a = other; return *this; }
};
```

## Why this is a bug
`operator=(const atomic_wrapper&)` is declared to return `atomic_wrapper&` but its body has no `return` statement. Flowing off the end of a non-void function is undefined behavior if the caller uses the returned reference (the sibling `operator=(const T&)` on the next line correctly does `return *this;`). Compilers warn (`-Wreturn-type`), and with optimization the missing return can yield a garbage reference.

`atomic_wrapper<bool> failed` is a member of `DownloadURL`, which is stored in `std::vector<DownloadURL> roundRobin.urls`. Any operation that copy-*assigns* a `DownloadURL` (as opposed to copy-constructs it) invokes this operator; if its result is ever read, behavior is undefined. It is latent today because the vector is only `push_back`-grown (copy/move construction) and assignments discard the result, but it is a real defect.

## Suggested fix
```cpp
atomic_wrapper &operator=(const atomic_wrapper &other) { _a.store(other._a.load()); return *this; }
```
