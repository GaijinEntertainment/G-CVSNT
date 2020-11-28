#include <stdio.h>
#include <string>
size_t blob_get_file_size(const char* fn)
{
  FILE *f = fopen(fn, "rb");
  if (!f)
    return 0;
  fseek(f, 0, SEEK_END);
  size_t fsz = ftell(f);
  fclose(f);
  return fsz;
}
bool blob_is_file_readable(const char* fn)
{
  FILE *f = fopen(fn, "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}

bool blob_rename_file(const char*from, const char*to)
{
  return rename(from, to) >= 0;
}

void blob_unlink_file(const char* f)
{
  _unlink(f);
}

FILE* blob_get_temp_file(std::string &fn)
{
  fn = "tmp";
  return fopen(fn.c_str(), "wb");
}