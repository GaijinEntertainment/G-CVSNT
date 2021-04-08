#define _FILE_OFFSET_BITS 64

#include "fileio.h"
#include <stdio.h>
#if !_MSC_VER
#include <unistd.h>
#endif

uint64_t blob_fwrite64(const void *data_, size_t esz, uint64_t cnt, FILE *f)
{
  uint64_t written = 0;
  const char *data = (const char *)data_;
  const size_t quant = 1073741824;//1Gb
  while (cnt >= quant)
  {
    const size_t wr = fwrite(data, esz, quant, f);
    written += (uint64_t)wr;
    if (wr != quant)
      return written;
    cnt -= quant;
    data += quant;
  }
  written += (uint64_t)fwrite(data, esz, (size_t)cnt, f);
  return written;
}

#if _MSC_VER

#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
//#include <wstring>
static std::wstring to_wstring(const char* ansiString)
{
  std::wstring returnValue;
  auto wideCharSize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, ansiString, -1, nullptr, 0);
  if (wideCharSize == 0)
    return returnValue;
  //#define LONG_PREFIX L"\\\\?\\"
  //const size_t prefixlen = wcslen(LONG_PREFIX);
  //returnValue = LONG_PREFIX;
  const size_t prefixlen = 0;
  returnValue.resize(wideCharSize + prefixlen);
  wideCharSize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, ansiString, -1, &returnValue[prefixlen], wideCharSize-prefixlen)+prefixlen;
  if (wideCharSize == 0)
  {
    returnValue.resize(0);
    return returnValue;
  }
  returnValue.resize(wideCharSize-1);
  return returnValue;
}

int blob_fileio_get_last_error()
{
  return GetLastError();
}


static bool blob_fileio_rename_file_a(const char*from, const char*to)//the only common implementation on posix and windows
{
  //the only atomic rename on windows which is portable
  //https://stackoverflow.com/questions/167414/is-an-atomic-file-rename-with-overwrite-possible-on-windows
  //https://github.com/golang/go/issues/8914
  //MoveFileTransacted is discouraged to use
  //MoveFileEx isn't atomic, if destination file exist
  //the only correct way to do it, is SetFileInformationByHandle, but it requires wide char conversion, so for now we keep current code
  //if dest file not exist, this is atomic (we do not allow copy)
  if (MoveFileExA(from, to, MOVEFILE_WRITE_THROUGH) == TRUE)
    return true;

  //if dest file exists, this is also atomic (we do not allow copy).
  //however, it has an issue - dest file won't be opening during renaming, resulting in potential race
  //We don't use production servers on windows, so we are fine.
  //if you ever want to do that, replace that with SetFileInformationByHandle
  return ReplaceFileA(to, from, NULL, 0, 0, 0) == TRUE;
}

bool blob_fileio_rename_file(const char*from_, const char*to_)//the only common implementation on posix and windows
{
  if (strlen(from_) < MAX_PATH && strlen(to_) < MAX_PATH)
    return blob_fileio_rename_file_a(from_, to_);
  std::wstring from = to_wstring(from_), to = to_wstring(to_);
  //the only atomic rename on windows which is portable
  //https://stackoverflow.com/questions/167414/is-an-atomic-file-rename-with-overwrite-possible-on-windows
  //https://github.com/golang/go/issues/8914
  //MoveFileTransacted is discouraged to use
  //MoveFileEx isn't atomic, if destination file exist
  //the only correct way to do it, is SetFileInformationByHandle, but it requires wide char conversion, so for now we keep current code
  //if dest file not exist, this is atomic (we do not allow copy)
  if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH) == TRUE)
    return true;

  //if dest file exists, this is also atomic (we do not allow copy).
  //however, it has an issue - dest file won't be opening during renaming, resulting in potential race
  //We don't use production servers on windows, so we are fine.
  //if you ever want to do that, replace that with SetFileInformationByHandle
  return ReplaceFileW(to.c_str(), from.c_str(), NULL, 0, 0, 0) == TRUE;
}


uint64_t blob_fileio_get_file_size(const char* fn)
{
  struct __stat64 buf;
  if (_stat64(fn, &buf) != 0)
      return invalid_blob_file_size; // error, could use errno to find out more

  return buf.st_size;
}

bool blob_fileio_is_file_readable(const char* fn)
{
  DWORD fa;
  if (strlen(fn) < MAX_PATH)
    fa = GetFileAttributesA(fn);
  else
    fa = GetFileAttributesW(to_wstring(fn).c_str());
  if (fa == INVALID_FILE_ATTRIBUTES)
	return false;
  return true;
}


void blob_fileio_unlink_file(const char* f) { _unlink(f); }

bool blob_fileio_ensure_dir(const char* name)
{
  if (strlen(name) < MAX_PATH)
    return CreateDirectoryA(name, NULL) != FALSE;
  else
    return CreateDirectoryW(to_wstring(name).c_str(), NULL) != FALSE;
}


const void* blob_fileio_os_mmap(const char *fp, std::uintmax_t flen)
{
  HANDLE fh =
    strlen(fp)<MAX_PATH ? CreateFileA(fp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0) :
           CreateFileW(to_wstring(fp).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (fh == INVALID_HANDLE_VALUE)
    return nullptr;

  HANDLE hmap = CreateFileMappingA(fh, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!hmap)
  {
    CloseHandle(fh);
    return nullptr;
  }
  void* ret = MapViewOfFileEx(hmap, FILE_MAP_READ, 0, 0, flen, NULL);
  CloseHandle(hmap);
  CloseHandle(fh);
  return ret;
}

void blob_fileio_os_unmap(const void* start, std::uintmax_t length)
{
  if (!start)
    return;
  (void) length;
  UnmapViewOfFile((void*)start);
}

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/errno.h>

int blob_fileio_get_last_error()
{
  return errno;
}

bool blob_fileio_rename_file(const char*from, const char*to)
{
  return rename(from, to) >= 0;
}

uint64_t blob_fileio_get_file_size(const char* fn)
{
  struct stat sb;
  if (stat(fn, &sb) == -1)
    return invalid_blob_file_size;
  return sb.st_size;
}


bool blob_fileio_is_file_readable(const char* fn)
{
  return access(fn, R_OK) == 0;
}


void blob_fileio_unlink_file(const char* f) { unlink(f); }

bool blob_fileio_ensure_dir(const char* name)
{
  int ret = mkdir(name, 0777);
  if (ret == 0)
    return true;
  if (ret == -1 && errno == EEXIST)
    return true;
  return false;
}


const void* blob_fileio_os_mmap(const char *filepath, std::uintmax_t flen)
{
  int fd = open(filepath, O_RDONLY);
  if (fd == -1)
    return nullptr;
  void *ret = mmap(NULL, flen, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
  close(fd);
  return ret;
}

void blob_fileio_os_unmap(const void* start, std::uintmax_t length)
{
  if (!start)
    return;
  munmap((void*)start, length);
}

#endif

#if _MSC_VER

FILE* blob_fileio_get_temp_file (std::string &fn, const char *tmp_path, const char *mode)
{
  char tempdir[_MAX_PATH];
  if (tmp_path == nullptr && !GetTempPathA(sizeof(tempdir), tempdir))
    return nullptr;
  char tempFileName[_MAX_PATH];
  if (!GetTempFileNameA(tmp_path ? tmp_path : tempdir, "hash_blob",0,tempFileName))
    return nullptr;
  fn = tempFileName;
  FILE *f;
  if (fopen_s(&f, fn.c_str(), mode ? mode : "wb") != 0)
    return nullptr;
  return f;
}

#else

FILE* blob_fileio_get_temp_file (std::string &fn, const char *tmp_path, const char *mode)
{
  FILE *fp = nullptr;

  char buf[1024];
  std::snprintf(buf, sizeof(buf), "%s/hash_blobXXXXXX", tmp_path ? tmp_path : "/tmp");
  int fd = mkstemp (buf);
  if (fd == -1)
    return nullptr;
  if ((fp = fdopen (fd, mode = mode ? mode : "wb+")) == NULL)
  {
    close (fd);
    unlink(buf);
  } else
  {
    chmod(buf, 0666);//linux create temp files with very limited access writes
    fn = buf;
  }
  return fp;
}

#endif
//memory mapped files

class BlobFileIOPullData
{
public:
  const char* begin;
  uint64_t size;
};

BlobFileIOPullData* blobe_fileio_start_pull(const char* filepath, uint64_t &blob_sz)
{
  blob_sz = blob_fileio_get_file_size(filepath);
  if (blob_sz == invalid_blob_file_size)
    return 0;
  if (blob_sz == 0)
    return new BlobFileIOPullData{ nullptr, blob_sz };

  const char *begin = (const char *) blob_fileio_os_mmap(filepath, blob_sz);
  if (!begin)
    return nullptr;
  return new BlobFileIOPullData{begin, blob_sz};
}

const char *blobe_fileio_pull(BlobFileIOPullData* fp, uint64_t from, uint64_t &data_pulled)
{
  if (!fp)
    return nullptr;
  const int64_t left = fp->size - from;
  if (left < 0)
    return nullptr;
  data_pulled = left;
  return fp->begin;
}

bool blobe_fileio_destroy(BlobFileIOPullData* up)
{
  if (!up)
    return false;
  blob_fileio_os_unmap((void*)up->begin, up->size);
  delete up;
  return true;
}


