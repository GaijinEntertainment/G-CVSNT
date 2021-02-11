#include <string>
#include <filesystem>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#if _MSC_VER
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

uint64_t space_occupied(const char *dir)
{
  uint64_t space_occupied_now = 0;
  std::error_code ec;
  for ( const auto& p : fs::recursive_directory_iterator(dir) )
  {
    if (!p.is_regular_file(ec))
      continue;
    std::uintmax_t fsz = p.file_size(ec);
    if (fsz == static_cast<std::uintmax_t>(-1))
      continue;
    space_occupied_now += fsz;
  }
  return space_occupied_now;
}

uint64_t free_space( const char *dir, int64_t max_size )
{
  struct file_info
  {
    fs::path path;
    fs::file_time_type last_write_time;
    std::uintmax_t size;
  };
  std::vector<file_info> flist;flist.reserve(1024);
  int64_t space_occupied_now = 0;
  std::error_code ec;

  for ( const auto& p : fs::recursive_directory_iterator(dir) )
  {
    const auto& path = p.path();
    if (!fs::is_regular_file(path, ec))
      continue;
    std::uintmax_t fsz = fs::file_size(path, ec);
    if (fsz == static_cast<std::uintmax_t>(-1))
      continue;
    flist.emplace_back(file_info{path, fs::last_write_time(path), fsz});
    space_occupied_now += fsz;
  }
  if (max_size < 0)
    max_size = space_occupied_now < max_size ? 0 : space_occupied_now + max_size;

  if (space_occupied_now <= max_size)
    return 0;
  std::sort( flist.begin(), flist.end(), [](auto& a, auto& b) { return a.last_write_time < b.last_write_time ; });
  uint64_t freed = 0;
  for (auto &i:flist)
  {
    if (!fs::remove(i.path, ec))
      continue;
    freed += i.size;
    space_occupied_now -= i.size;
    if (space_occupied_now <= max_size)
      return freed;
  }
  return freed;
}