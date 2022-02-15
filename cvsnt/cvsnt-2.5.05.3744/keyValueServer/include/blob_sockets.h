#pragma once
#include <stdint.h>
#include "blobs_encryption.h"

struct evp_cipher_ctx_st;
struct BlobSocket
{
  uintptr_t opaque = uintptr_t(~(uintptr_t(0)));
  evp_cipher_ctx_st *encrypt = nullptr, *decrypt = nullptr;
};

void blob_close_encryption(BlobSocket &);
BlobSocket connect_to_server_blob_socket_no_auth(intptr_t raw_socket);
BlobSocket connect_to_server_blob_socket(intptr_t raw_socket, const uint8_t otp_page[otp_page_size]);
BlobSocket connect_to_client_blob_socket_no_auth(intptr_t raw_socket);
BlobSocket connect_to_client_blob_socket(intptr_t raw_socket, uint8_t otp_page[otp_page_size]);

inline bool is_valid(const BlobSocket &b){return b.opaque != uintptr_t(~(uintptr_t(0)));}
enum class IpType {LOCAL, PRIVATE, PUBLIC};
extern IpType blob_classify_ip(uint32_t ip);//checks if ip is in private network

extern bool blob_init_sockets();
extern void blob_close_sockets();
extern void blob_close_socket(BlobSocket &);
extern int blob_get_last_sock_error();
extern bool blob_set_non_blocking(BlobSocket &sock, bool on);
extern bool blob_socket_would_block(const BlobSocket &sock);
extern void blob_set_socket_def_options(BlobSocket &sock);
extern void blob_set_socket_no_delay(BlobSocket &sock, bool no_delay);
extern void enable_keepalive(BlobSocket &socket, bool keep_alive);
extern void blob_send_recieve_sock_timeout(BlobSocket &socket, int timeout_sec);
extern int recv(BlobSocket &socket, void *buf, int len);
extern int send_msg_no_signal(BlobSocket &socket, const void *buf, int len);
extern int connect_with_timeout(BlobSocket &sock, const struct sockaddr *addr, size_t addr_len, int timeout_sec);