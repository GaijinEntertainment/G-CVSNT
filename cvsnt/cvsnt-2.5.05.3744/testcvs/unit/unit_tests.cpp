// Unit tests for the header-resident parts of the content-addressed blob layer.
//
// These cover code that is reachable without a repository, a server or a
// socket: the blob header format, the streaming header accumulator, and the
// wire hash encoding.  Each of them has had a real defect, so each is worth a
// permanent test.
//
// Build: see testcvs/README.md for the working Linux/macOS and Windows
// command lines (from this directory the include roots are ../.., and the
// Windows build needs /MD to match the in-tree zlib/zstd CRT).
//
// Exit status is 0 if every check passed.

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "ca_blob_format.h"
#include "streaming_blobs.h"
#include "blob_hash_util.h"
#include "blob_push_protocol.h"

static int g_checks = 0;
static int g_failures = 0;
static const char *g_case = "<none>";

static void check(bool cond, const char *what)
{
  ++g_checks;
  if (!cond)
  {
    ++g_failures;
    std::printf("  FAIL  %s: %s\n", g_case, what);
  }
}

template <typename A, typename B>
static void check_eq(const A &got, const B &want, const char *what)
{
  ++g_checks;
  if (!(got == want))
  {
    ++g_failures;
    std::printf("  FAIL  %s: %s\n", g_case, what);
  }
}

#define CASE(name) g_case = name

// --------------------------------------------------------------------------- blob header

static void test_blob_header_layout()
{
  CASE("blob header layout");
  // The on-disk format depends on this being exactly 16 bytes with these
  // offsets; a compiler that padded differently would silently corrupt every
  // blob written by that build.
  check_eq(sizeof(caddressed_fs::BlobHeader), (size_t)16, "BlobHeader must be 16 bytes");
  check_eq(offsetof(caddressed_fs::BlobHeader, magic), (size_t)0, "magic at offset 0");
  check_eq(offsetof(caddressed_fs::BlobHeader, headerSize), (size_t)4, "headerSize at offset 4");
  check_eq(offsetof(caddressed_fs::BlobHeader, flags), (size_t)6, "flags at offset 6");
  check_eq(offsetof(caddressed_fs::BlobHeader, uncompressedLen), (size_t)8, "uncompressedLen at offset 8");
}

static void test_blob_header_magic()
{
  CASE("blob header magic");
  using namespace caddressed_fs;
  BlobHeader h = get_header(noarc_magic, 1234, 0);
  check(is_accepted_magic(h.magic), "NONE is an accepted magic");
  check(is_noarc_blob(h), "NONE recognised as unpacked");
  check(!is_packed_blob(h), "NONE is not packed");
  check_eq(h.uncompressedLen, (uint64_t)1234, "uncompressedLen round trip");
  check_eq(h.headerSize, (uint16_t)sizeof(BlobHeader), "headerSize is set");

  h = get_header(zstd_magic, 7, BlobHeader::BEST_POSSIBLE_COMPRESSION);
  check(is_zstd_blob(h), "ZSTD recognised");
  check(is_packed_blob(h), "ZSTD is packed");
  check_eq(h.flags, (uint16_t)BlobHeader::BEST_POSSIBLE_COMPRESSION, "flags round trip");

  h = get_header(zlib_magic, 7, 0);
  check(is_zlib_blob(h), "ZLIB recognised");
  check(is_packed_blob(h), "ZLIB is packed");

  const unsigned char junk[4] = {'J','U','N','K'};
  BlobHeader bad = get_header(junk, 0, 0);
  check(!is_accepted_magic(bad.magic), "an unknown magic is rejected");
}

// --------------------------------------------------------------------- streaming header

// Feeds a whole unpacked blob through decode_stream_blob_data() in fixed-size
// chunks and returns the reassembled body, or false if decoding failed.
//
// The point of the test is the *header accumulator*: the 16-byte header can be
// split across any number of chunks, and the arithmetic that tracks how many
// header bytes are still outstanding has to cope with every split.
static bool feed_in_chunks(const std::vector<char> &blob, size_t chunk,
                           std::vector<char> &body_out)
{
  caddressed_fs::DownloadBlobInfo info;
  body_out.clear();
  for (size_t off = 0; off < blob.size(); off += chunk)
  {
    size_t n = blob.size() - off;
    if (n > chunk)
      n = chunk;
    bool ok = caddressed_fs::decode_stream_blob_data(
        info, blob.data() + off, n,
        [&](const void *data, uint64_t sz)
        {
          const char *p = (const char *)data;
          body_out.insert(body_out.end(), p, p + (size_t)sz);
          return true;
        });
    if (!ok)
      return false;
  }
  return true;
}

static void test_stream_header_split()
{
  CASE("streaming header split across chunks");
  using namespace caddressed_fs;

  const size_t BODY = 100;
  std::vector<char> body(BODY);
  for (size_t i = 0; i < BODY; ++i)
    body[i] = (char)(i & 0xff);

  BlobHeader hdr = get_header(noarc_magic, BODY, 0);
  std::vector<char> blob(sizeof(BlobHeader) + BODY);
  std::memcpy(blob.data(), &hdr, sizeof(BlobHeader));
  std::memcpy(blob.data() + sizeof(BlobHeader), body.data(), BODY);

  // Every chunk size from 1 byte (header arrives one byte at a time) up to the
  // whole blob in one go must reassemble the body exactly.
  for (size_t chunk = 1; chunk <= blob.size(); ++chunk)
  {
    std::vector<char> got;
    bool ok = feed_in_chunks(blob, chunk, got);
    if (!ok)
    {
      char msg[96];
      std::snprintf(msg, sizeof(msg), "decode reported failure at chunk size %zu", chunk);
      check(false, msg);
      continue;
    }
    if (got.size() != BODY || std::memcmp(got.data(), body.data(), BODY) != 0)
    {
      char msg[96];
      std::snprintf(msg, sizeof(msg), "body mismatch at chunk size %zu (got %zu bytes)",
                    chunk, got.size());
      check(false, msg);
    }
    else
      check(true, "");
  }
}

static void test_stream_header_only()
{
  CASE("streaming header with an empty body");
  using namespace caddressed_fs;
  BlobHeader hdr = get_header(noarc_magic, 0, 0);
  std::vector<char> blob(sizeof(BlobHeader));
  std::memcpy(blob.data(), &hdr, sizeof(BlobHeader));

  std::vector<char> got;
  check(feed_in_chunks(blob, 3, got), "header-only blob decodes");
  check_eq(got.size(), (size_t)0, "header-only blob yields no body");
}

// ------------------------------------------------------------------------- hash encoding

static void test_hash_hex_round_trip()
{
  CASE("hash hex round trip");
  unsigned char bin[32];
  for (int i = 0; i < 32; ++i)
    bin[i] = (unsigned char)(i * 7 + 1);

  char hex[65];
  check(bin_hash_to_hex_string_s(bin, sizeof(bin), hex, sizeof(hex)), "bin -> hex succeeds");
  check_eq(std::strlen(hex), (size_t)64, "hex is 64 characters");

  unsigned char back[32];
  check(hex_string_to_bin_hash(hex, back), "hex -> bin succeeds");
  check_eq(std::memcmp(bin, back, 32), 0, "round trip preserves the hash");
}

static void test_blob_hash_encoding()
{
  CASE("wire hash encoding");
  // encode_hash_str_to_blob_hash() is the convenience wrapper over the _s
  // form.  It once called itself, which recursed until the stack ran out - so
  // simply reaching the assertions below is part of the test.
  unsigned char bin[32];
  for (int i = 0; i < 32; ++i)
    bin[i] = (unsigned char)(255 - i);
  char hex[65];
  bin_hash_to_hex_string_s(bin, sizeof(bin), hex, sizeof(hex));

  unsigned char wire[blob_push_proto::hash_len];
  std::memset(wire, 0xAB, sizeof(wire));
  check(encode_hash_str_to_blob_hash("blake3:", hex, wire), "encode succeeds");

  // Six bytes of type tag, then the raw hash: the trailing ':' is not sent.
  check_eq(std::memcmp(wire, "blake3", 6), 0, "type tag is the first six bytes");
  check_eq(std::memcmp(wire + 6, bin, 32), 0, "raw hash follows the tag");
  check_eq((size_t)blob_push_proto::hash_len, (size_t)(32 + 6), "hash_len is tag + hash");

  // A buffer one byte too small must be refused rather than overrun.
  unsigned char small[blob_push_proto::hash_len];
  check(!encode_hash_str_to_blob_hash_s("blake3:", hex, small, sizeof(small) - 1),
        "an undersized buffer is refused");
}

static void test_blob_hash_decoding()
{
  CASE("wire hash decoding");
  unsigned char bin[32];
  for (int i = 0; i < 32; ++i)
    bin[i] = (unsigned char)(i ^ 0x5a);
  char hex[65];
  bin_hash_to_hex_string_s(bin, sizeof(bin), hex, sizeof(hex));

  unsigned char wire[blob_push_proto::hash_len];
  check(encode_hash_str_to_blob_hash("blake3:", hex, wire), "encode succeeds");

  char type[8], hex_back[65];
  check(decode_blob_hash_to_hex_hash_s(wire, sizeof(wire), type, sizeof(type),
                                       hex_back, sizeof(hex_back)),
        "decode succeeds");
  check_eq(std::string(type), std::string("blake3"), "type tag decodes");
  check_eq(std::string(hex_back), std::string(hex), "hex hash decodes");
}

// --------------------------------------------------------------------------------- main

int main()
{
  test_blob_header_layout();
  test_blob_header_magic();
  test_stream_header_split();
  test_stream_header_only();
  test_hash_hex_round_trip();
  test_blob_hash_encoding();
  test_blob_hash_decoding();

  std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
