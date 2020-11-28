zero dependencies on CVS itself

it is merely a simple tcp server/client transfer file server, which serve some content-as-key storage, for any hash of 32 size (for example blake3 or sha256).
hash key as preambled with 7 bytes suffix in a form of "sha256:" or "blake3:"
  actually, the only thing is checked is this ":" at [6], so one can increase hash key length with 5-6 bytes (for example, add size of blob, or more bits of hash)
  However, sample implementation relies that all hashes are of one certain type

actual storage can be anything, it also allows memory mapped files (for performance).
  sample Implementation does mmap. It is easy (and very possible) to add cached proxy. (proxy should only use write-through cache).

==logging==
logging is done as link time dependency to

extern void blob_logmessage(int log, const char *msg,...);
it sample implementation is provided in sampleImplementation (if you use a static lib, you can override it, if you make shared librry - replace)

see def_log_printf.cpp


==clientLib==

pull has no link-time dependencies, except for logging

==serverLib==

serverLib starts thread per each request in semi-inifinte loop.

data handling is done as link time dependency to:

extern size_t blob_get_hash_blob_size(const char* full_hash_str);//hash_str is blake3:hex_encoded_hash
extern bool blob_does_hash_blob_exist(const char* full_hash_str);//hash_str is blake3:hex_encoded_hash

extern uintptr_t blob_start_push_data(const char* full_hash_str, uint64_t size);//hash_str is blake3:hex_encoded_hash
extern void blob_push_data(const void *data, size_t data_size, uintptr_t up);
extern bool blob_end_push_data(uintptr_t up);

extern uintptr_t blob_start_pull_data(const char *full_hash_str, uint64_t &blob_sz);//allows memory mapping. blob_sz is the size of blob. if invalid returns nullptr
extern const char *blob_pull_data(uintptr_t readBlob, uint64_t from, size_t &data_pulled);//data_pulled != 0, unless error
extern bool blob_end_pull_data(uintptr_t tempBlob);

there are sample implementations of these functions in sampleImplementation.


Sample:
====================================================

there are two sample applications: server and client
Server
To build server:
cd server
clang -O3 blob_file_server.cpp ../serverLib/blob_push_proc.cpp ../serverLib/blob_push_server.cpp ../sampleImplementation/blob_file_lib.cpp ../sampleImplementation/simple_file_lib.cpp ../sampleImplementation/fileio.cpp

Client
to build client:
cd client
clang -O3 blob_file_server_sample_client.cpp ../clientLib/blob_push_pull_client.cpp ../clientLib/blob_push_client_cmd.cpp ../clientLib/blob_pull_client_cmd.cpp ../clientLib/blob_chck_client_cmd.cpp 

===================================================

Building your own Application.
You actually need to implement all 8 functions in blob_server_func_deps.h, that's ALL;

To override functions/replace blob_file_lib.cpp.

For example, if you have all data in memory, it will be some key/value storage, without file access.

If you are making proxy server, with file cache, you will still need simple_file_lib.cpp, but it will be called directly, not from blob_file_lib;

simple_file_lib uses abstracted file IO, and it can be also replaced by implementing all functions in file_functions.h (and removing fileio.cpp);
