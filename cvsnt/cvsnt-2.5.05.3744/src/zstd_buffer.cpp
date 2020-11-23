#include "cvs.h"
#include "buffer.h"

#include "zstd.h"

struct zstd_buffer
{
  /* The underlying buffer.  */
  struct buffer *buf;
  /* The compression information.  */
  union {
    ZSTD_DStream* zds;
	ZSTD_CStream* zcs;
  };
  ZSTD_outBuffer_s streamOut;
  ZSTD_inBuffer_s streamIn;
};

static void zstd_error (int, size_t, const char *);
static int zstd_buffer_input (void *, char *, int, int, int *);
static int zstd_buffer_output (void *, const char *, int, int *);
static int zstd_buffer_flush (void *);
static int zstd_buffer_block (void *, int);
static int zstd_buffer_shutdown_input (void *);
static int zstd_buffer_shutdown_output (void *);

/* Report an error from one of the zlib functions.  */

static void zstd_error (int status, size_t zstatus, const char *msg)
{
  error (status, 0, "%s: %s", msg, ZSTD_getErrorName(zstatus));
}

/* Create a compression buffer.  */

struct buffer * zstd_buffer_initialize (struct buffer *buf, int input, int level, void (*memory)(struct buffer *))
{
  struct zstd_buffer *n;
  int zstatus;

  n = (struct zstd_buffer *) xmalloc (sizeof *n);
  memset (n, 0, sizeof *n);

  n->buf = buf;

  if (input)
  {
    n->zds = ZSTD_createDStream();
    ZSTD_initDStream(n->zds);
  } else
  {
    n->zcs = ZSTD_createCStream();
    ZSTD_initCStream(n->zcs, level <= 1 ? -1 : level - 1);
  }
  /* There may already be data buffered on BUF.  For an output
     buffer, this is OK, because these routines will just use the
     buffer routines to append data to the (uncompressed) data
     already on BUF.  An input buffer expects to handle a single
     buffer_data of buffered input to be uncompressed, so that is OK
     provided there is only one buffer.  At present that is all
     there ever will be; if this changes, zstd_buffer_input must
     be modified to handle multiple input buffers.  */
  assert (! input || buf->data == NULL || buf->data->next == NULL);

  return buf_initialize (input ? zstd_buffer_input : NULL,
  		   input ? NULL : zstd_buffer_output,
  		   input ? NULL : zstd_buffer_flush,
  		   zstd_buffer_block,
  		   (input
  		    ? zstd_buffer_shutdown_input
  		    : zstd_buffer_shutdown_output),
  		   memory,
  		   n);
}

/* Input data from a compression buffer.  */

static int zstd_buffer_input (void *closure, char *data, int need, int size, int *got)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;
  struct buffer_data *bd;

  if (cb->buf->input == NULL)
  	abort ();

  /* We use a single buffer_data structure to buffer up data which
     the z_stream structure won't use yet.  We can safely store this
     on cb->buf->data, because we never call the buffer routines on
     cb->buf; we only call the buffer input routine, since that
     gives us the semantics we want.  As noted in
     zstd_buffer_initialize, the buffer_data structure may
     already exist, and hold data which was already read and
     buffered before the decompression began.  */
  bd = cb->buf->data;
  if (bd == NULL)
  {
    bd = ((struct buffer_data *) xmalloc (sizeof (struct buffer_data)));
    if (bd == NULL)
      return -2;
    bd->text = (char *) xmalloc (BUFFER_DATA_SIZE);
    if (bd->text == NULL)
    {
      xfree (bd);
      return -2;
    }
    bd->bufp = bd->text;
    bd->size = 0;
    cb->buf->data = bd;
  }

  cb->streamOut.dst = data;
  cb->streamOut.size = size;
  cb->streamOut.pos = 0;

  while (1)
  {
    int zstatus, sofar, status, nread;

    /* First try to inflate any data we already have buffered up.
       This is useful even if we don't have any buffered data,
       because there may be data buffered inside the z_stream
       structure.  */

    cb->streamIn.src = bd->bufp;
    cb->streamIn.size = bd->size;
    cb->streamIn.pos = 0;

    while (cb->streamIn.pos < cb->streamIn.size && cb->streamOut.pos < cb->streamOut.size)
    {
      size_t sz = ZSTD_decompressStream(cb->zds, &cb->streamOut, &cb->streamIn);
      if (ZSTD_isError(sz))
      {
      	zstd_error (0, sz, "inflate");
      	return EIO;
      }
    };

    bd->size = cb->streamIn.size - cb->streamIn.pos;
    bd->bufp = (char *)cb->streamIn.src + cb->streamIn.pos;

    /* If we have obtained NEED bytes, then return, unless NEED is
           zero and we haven't obtained anything at all.  If NEED is
           zero, we will keep reading from the underlying buffer until
           we either can't read anything, or we have managed to
           inflate at least one byte.  */
    sofar = size - (cb->streamOut.size - cb->streamOut.pos);
    if (sofar > 0 && sofar >= need)
      break;

    //if (zstatus == Z_STREAM_END)
     //   return -1;

    /* All our buffered data should have been processed at this
           point.  */
    assert (bd->size == 0);

    /* This will work well in the server, because this call will
       do an unblocked read and fetch all the available data.  In
       the client, this will read a single byte from the stdio
       stream, which will cause us to call inflate once per byte.
       It would be more efficient if we could make a call which
       would fetch all the available bytes, and at least one byte.  */

    status = (*cb->buf->input) (cb->buf->closure, bd->text,
    			    need > 0 ? 1 : 0,
    			    BUFFER_DATA_SIZE, &nread);
    if (status != 0)
      return status;

    /* If we didn't read anything, then presumably the buffer is
           in nonblocking mode, and we should just get out now with
           whatever we've inflated.  */
    if (nread == 0)
    {
        assert (need == 0);
        break;
    }

    bd->bufp = bd->text;
    bd->size = nread;
  }

  *got = cb->streamOut.pos;

  return 0;
}

/* Output data to a compression buffer.  */

static int zstd_buffer_output (void *closure, const char *data, int have, int *wrote)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;

  cb->streamIn.src = (unsigned char *) data;
  cb->streamIn.size = have;
  cb->streamIn.pos = 0;

  while (cb->streamIn.pos < cb->streamIn.size)
  {
    char buffer[BUFFER_DATA_SIZE];
    int zstatus;

    cb->streamOut.dst = (unsigned char *) buffer;
    cb->streamOut.size = sizeof(buffer);
    cb->streamOut.pos = 0;

    size_t sz = ZSTD_compressStream(cb->zcs, &cb->streamOut, &cb->streamIn);
    if (ZSTD_isError(sz))
    {
      zstd_error (0, sz, "inflate");
      return EIO;
    }

    if (cb->streamOut.pos != 0)
      buf_output (cb->buf, buffer, cb->streamOut.pos);
  }

  *wrote = have;

  /* We will only be here because buf_send_output was called on the
     compression buffer.  That means that we should now call
     buf_send_output on the underlying buffer.  */
  return buf_send_output (cb->buf);
}

/* Flush a compression buffer.  */

static int zstd_buffer_flush (void *closure)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;


  while (1)
  {
    char buffer[BUFFER_DATA_SIZE];
    cb->streamOut.dst  = (unsigned char *) buffer;
    cb->streamOut.size = sizeof(buffer);
    cb->streamOut.pos = 0;
    size_t sz = ZSTD_flushStream(cb->zcs, &cb->streamOut);

    if (ZSTD_isError(sz))
    {
        zstd_error (0, sz, "deflate flush");
        return EIO;
    }

    if (cb->streamOut.pos != 0)
        buf_output (cb->buf, buffer, cb->streamOut.pos);

    /* If the deflate function did not fill the output buffer,
           then all data has been flushed.  */
    if (cb->streamOut.pos < cb->streamOut.size)
        break;
  }

  /* Now flush the underlying buffer.  Note that if the original
     call to buf_flush passed 1 for the BLOCK argument, then the
     buffer will already have been set into blocking mode, so we
     should always pass 0 here.  */
  return buf_flush (cb->buf, 0);
}

/* The block routine for a compression buffer.  */

static int zstd_buffer_block (void *closure, int block)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;

  if (block)
	return set_block (cb->buf);
  else
	return set_nonblock (cb->buf);
}

/* Shut down an input buffer.  */

static int zstd_buffer_shutdown_input (void *closure)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;
  size_t sz = ZSTD_freeDStream(cb->zds);
  if (ZSTD_isError(sz))
  {
    zstd_error (0, sz, "inflateEnd");
    return EIO;
  }

  return buf_shutdown (cb->buf);
}

/* Shut down an output buffer.  */

static int zstd_buffer_shutdown_output (void *closure)
{
  struct zstd_buffer *cb = (struct zstd_buffer *) closure;

  while (1)
  {
    char buffer[BUFFER_DATA_SIZE];
    cb->streamOut.dst= (unsigned char *) buffer;
    cb->streamOut.size = sizeof(buffer);
    cb->streamOut.pos = 0;
    size_t sz = ZSTD_endStream(cb->zcs, &cb->streamOut);

      break;

    if (ZSTD_isError(sz))
    {
        zstd_error (0, sz, "deflate flush");
        return EIO;
    }

    if (cb->streamOut.pos != 0)
        buf_output (cb->buf, buffer, cb->streamOut.pos);

    /* If the deflate function did not fill the output buffer,
           then all data has been flushed.  */
    if (cb->streamOut.pos < cb->streamOut.size)
        break;
  }

/* Now flush the underlying buffer.  Note that if the original
   call to buf_flush passed 1 for the BLOCK argument, then the
   buffer will already have been set into blocking mode, so we
   should always pass 0 here.  */
  int status = buf_flush (cb->buf, 0);
  if (status != 0)
    return status;

  return buf_shutdown (cb->buf);
}
