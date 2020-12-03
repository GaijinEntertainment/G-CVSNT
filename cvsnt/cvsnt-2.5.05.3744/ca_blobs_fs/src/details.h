#pragma once
#include <string>
namespace caddressed_fs
{
  std::string get_file_path(const char* hash_hex_string);//only exposed for tools, after root is set
  std::string blobs_dir_path();//only exposed for tools, after root is set (returns root/blobs)
  const char* blobs_subfolder();//returns "blobs"
  int64_t repack(const char* hash_hex_string);//only exposed for tools. This call ensures that blob is compressed with best possible compression. returns size of difference (CAN be negative is zlib changed to zstd. zstd is faster)
}