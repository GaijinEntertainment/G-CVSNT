// Standalone unit tests for the content-addressed blob store (ca_blobs_fs).
//
// Exercises the real library: push (framed via push_whole_blob_from_raw_data),
// content-addressed dedup, existence, and a pull round-trip; plus the audited
// fix that set_root(ctx, nullptr) must honor the documented "use the default
// root" contract instead of crashing (fix: blob-008).
//
// Build: see tests/README.md for the exact cl command.
// Exit code 0 = all passed, non-zero = a failure (prints which).

#include "../content_addressed_fs.h"
#include "../calc_hash.h"
#include "../push_whole_blob.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
using namespace caddressed_fs;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FAIL: %s\n", (msg)); ++g_failures; } \
    else         { std::printf("ok  : %s\n", (msg)); } } while (0)

// Push raw bytes as a blob; returns the 64-char hex hash (NUL-terminated).
static bool push_raw(context *ctx, const void *data, size_t sz, char hash65[65], bool pack)
{
  std::memset(hash65, 0, 65);
  bool ok = push_whole_blob_from_raw_data(ctx, data, sz, hash65, pack);
  hash65[64] = 0;
  return ok;
}

int main()
{
  std::error_code ec;
  const fs::path base = fs::temp_directory_path(ec) / "cafs_unit_test";
  fs::remove_all(base, ec);
  const fs::path root = base / "root";
  // The store does not create its own blobs/ base dir (callers mkdir it).
  fs::create_directories(root / "blobs", ec);

  context *ctx = create();
  CHECK(ctx != nullptr, "create() returns a context");

  // blob-008: nullptr root must not crash (documented "use the default root").
  set_root(ctx, nullptr);
  CHECK(true, "set_root(ctx, nullptr) did not crash");

  set_root(ctx, root.string().c_str());
  set_allow_trust(true);

  const char payloadA[] = "the quick brown fox jumps over the lazy dog, 12345";
  const size_t plenA = sizeof(payloadA) - 1;
  char hashA[65];
  CHECK(push_raw(ctx, payloadA, plenA, hashA, /*pack*/false), "push blob A (unpacked)");
  CHECK(std::strlen(hashA) == 64, "push returns a 64-char hex hash");
  CHECK(exists(ctx, hashA), "exists() finds blob A");
  CHECK(get_size(ctx, hashA) != invalid_size, "get_size() reports a size for blob A");

  // Content addressing: pushing identical bytes again yields the same hash and
  // does not error (dedup fast path).
  char hashA2[65];
  CHECK(push_raw(ctx, payloadA, plenA, hashA2, false), "re-push identical content");
  CHECK(std::strcmp(hashA, hashA2) == 0, "identical content hashes identically (dedup)");

  // Different content -> different hash; both coexist.
  const char payloadB[] = "a completely different payload, packed with zstd";
  char hashB[65];
  CHECK(push_raw(ctx, payloadB, sizeof(payloadB) - 1, hashB, /*pack*/true), "push blob B (zstd)");
  CHECK(std::strcmp(hashA, hashB) != 0, "different content hashes differently");
  CHECK(exists(ctx, hashA) && exists(ctx, hashB), "both blobs exist after distinct pushes");

  // A hash that was never pushed must not exist.
  const char missing[65] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  CHECK(!exists(ctx, missing), "exists() is false for an unknown hash");

  // Pull round-trip sanity: the stored blob is readable and non-empty.
  {
    uint64_t sz = 0;
    PullData *pl = start_pull(ctx, hashA, sz);
    CHECK(pl != nullptr, "start_pull opens blob A");
    if (pl)
    {
      uint64_t got = 0;
      const char *data = pull(pl, 0, got);
      CHECK(data != nullptr && got > 0, "pull() returns non-empty data");
      destroy(pl);
    }
  }

  destroy(ctx);
  fs::remove_all(base, ec);

  std::printf("\n%s (%d failure%s)\n", g_failures ? "TESTS FAILED" : "ALL TESTS PASSED",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
