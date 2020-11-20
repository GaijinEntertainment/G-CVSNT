#include "cvs.h"
#include "sha_blob_reference.h"

size_t decode_binary_blob(const char *context, const void *data, size_t fileLen, void **out_data, bool &need_free)
{
  need_free = false;
  if (fileLen < sizeof(BlobHeader))
  {
    error(1,errno,"Couldn't read %s of %d len", context, (int)fileLen);
    return 0;
  }
  const BlobHeader &hdr = *(const BlobHeader *) data;
  if (!is_accepted_magic(hdr.magic) || hdr.compressedLen != fileLen - sizeof(hdr))
  {
    error(1,errno,"<%s> received not a blob (%d bytes stored in file, vs file len = %d)", context, (int)hdr.compressedLen, int(fileLen - sizeof(hdr)));
    return 0;
  }
  const char* readData = ((const char*)data) + hdr.headerSize;

  if (is_packed_blob(hdr))//
  {
    *out_data = malloc(hdr.uncompressedLen);
    need_free = true;
    if (is_zlib_blob(hdr))
    {
      z_stream stream = {0};
      inflateInit(&stream);
      stream.avail_in = hdr.compressedLen;
      stream.next_in = (Bytef*)readData;
      stream.avail_out = hdr.uncompressedLen;
      stream.next_out = (Bytef*)*out_data;
      if(inflate(&stream, Z_FINISH)!=Z_STREAM_END)
          error(1,0,"internal error: inflate failed");
      inflateEnd(&stream);
    } else
    {
      error(1,errno,"Unknown compressor %s", context);
      return 0;
    }
  } else
    *out_data = (void*)readData;
  return hdr.uncompressedLen;
}
