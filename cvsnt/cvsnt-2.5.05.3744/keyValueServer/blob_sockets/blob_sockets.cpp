#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include "../include/blob_sockets.h"
#include "../include/blob_raw_sockets.h"
#include <openssl/rand.h>


//we are using CFB mode, which is blocked
#define CIPHER_BLOCK_SZ 1

IpType blob_classify_ip(uint32_t ip)
{
  if (ip == 0x100007f)//127.0.0.1
    return IpType::LOCAL;
  //https://en.wikipedia.org/wiki/Private_network
  if ( (ip&0xF) == 10)//10.x.x.x
    return IpType::PRIVATE;
  if ( (ip&0xFFFF) == 0xa8c0)//192.168.x.x
    return IpType::PRIVATE;
  if ( (ip&0xFFF) >= 0x10ac && (ip&0xFFF) <= 0x1fac)//172.16.x.x -- 172.31.x.x
    return IpType::PRIVATE;
  return IpType::PUBLIC;;
}

bool blob_init_sockets() {
  ERR_load_CRYPTO_strings();
  OPENSSL_add_all_algorithms_noconf();
  return raw_init_sockets();
}

void blob_close_sockets(){ raw_close_sockets();}

void blob_close_encryption(BlobSocket& blob_socket) {
  if (blob_socket.encrypt)
  {
    EVP_CIPHER_CTX_free(blob_socket.encrypt);
  }
  if (blob_socket.decrypt)
  {
    EVP_CIPHER_CTX_free(blob_socket.decrypt);
  }
}

void blob_close_socket(BlobSocket &blob_socket) {
  blob_close_encryption(blob_socket);
  intptr_t socket = blob_socket.opaque;
  raw_close_socket(socket);
  blob_socket = BlobSocket();
}
int blob_get_last_sock_error() { return raw_get_last_sock_error(); }

void blob_set_socket_def_options(BlobSocket &blob_socket)
{
  raw_set_socket_def_options(blob_socket.opaque);
}

void blob_set_socket_no_delay(BlobSocket &blob_socket, bool no_delay)
{
  raw_set_socket_no_delay(blob_socket.opaque, no_delay);
}

void blob_send_recieve_sock_timeout(BlobSocket &blob_socket, int timeout_sec) {
  raw_send_recieve_sock_timeout(blob_socket.opaque, timeout_sec);
}

static int decrypt_and_finalize(evp_cipher_ctx_st *cipher, unsigned char *to, size_t space_left, const unsigned char *from, int encrypted_len)
{
  int outLen;
  if (1 != EVP_DecryptUpdate(cipher, to, &outLen, from, encrypted_len) || outLen > space_left || outLen > encrypted_len)
    return -1;
#if CIPHER_BLOCK_SZ != 1
  int finalLen;
  if (1 != EVP_DecryptFinal_ex(cipher, to + outLen, &finalLen) || outLen + finalLen > space_left)
    return -1;
  return outLen + finalLen;
#else
  return outLen;
#endif
}

static int encrypt_and_finalize(evp_cipher_ctx_st *cipher, unsigned char *to, size_t space_left, const unsigned char *from, int decrypted_len)
{
  int outLen;
  if (1 != EVP_EncryptUpdate(cipher, to, &outLen, from, decrypted_len) || outLen > space_left || outLen > decrypted_len)
    return -1;
#if CIPHER_BLOCK_SZ != 1
  int finalLen;
  if (1 != EVP_EncryptFinal_ex(cipher, to + outLen, &finalLen) || outLen + finalLen > space_left)
    return -1;
  return outLen + finalLen;
#else
  return outLen;
#endif
}

int recv(BlobSocket &socket, void *buf_, int len)
{
  if (!socket.decrypt)
    return recv((SOCKET)socket.opaque, (char*)buf_, len, 0);
  unsigned char* buf = (unsigned char*)buf_;
  const int lenRead = ((len+CIPHER_BLOCK_SZ-1)&~(CIPHER_BLOCK_SZ-1));
  int read = 0, decrypted = 0;
  for (int lenLeft = lenRead; lenLeft > 0;)
  {
    unsigned char decryptedBuf[32768];//decode by 32768
    const int curLen = lenLeft < sizeof(decryptedBuf) ? lenLeft : (int)sizeof(decryptedBuf);
    const int ret = (int)raw_recv_exact((SOCKET)socket.opaque, (char*)decryptedBuf, curLen);
    if (ret <= 0)
      return ret;
    const int curDecrypted = decrypt_and_finalize(socket.decrypt, buf+decrypted, len-decrypted, decryptedBuf, curLen);
    if (curDecrypted < 0)
      return -1;
    decrypted += curDecrypted;
    lenLeft -= ret;
  }
  return decrypted;
}

static int send(BlobSocket &socket, const void *buf_, int len, int flags)
{
  if (!socket.encrypt)
    return send((SOCKET)socket.opaque, (const char*)buf_, len, flags);

  const unsigned char* buf = (const unsigned char*)buf_;
  unsigned char encryptedBuf[32768];//encode by 32768
  int encrypted = 0;
  for (int lenLeft = len; lenLeft > 0;)
  {
    const int curLen = sizeof(encryptedBuf) < lenLeft ? (int)sizeof(encryptedBuf) : lenLeft;
    const int curEncrypted = encrypt_and_finalize(socket.encrypt, encryptedBuf, sizeof(encryptedBuf), buf, curLen);
    if (curEncrypted < 0)
      return -1;
    encrypted += curEncrypted;
    buf += curLen;
    lenLeft -= curLen;
    const int ret = send((SOCKET)socket.opaque, (const char*)encryptedBuf, curEncrypted, flags);
    if (ret <= 0)
      return ret;
  }
  if (encrypted != ((len + CIPHER_BLOCK_SZ-1)&~(CIPHER_BLOCK_SZ-1)))
    return -1;
  return len;
}

int send(BlobSocket &socket, const void *buf, int len)
{
  return send(socket, buf, len, 0);
}

int send_msg_no_signal(BlobSocket &socket, const void *buf, int len)
{
  return send(socket, buf, len, MSG_NOSIGNAL);
}

static bool create_encryption_pair(uint8_t encrypt_key[key_plus_iv_size], uint8_t decrypt_key[key_plus_iv_size], evp_cipher_ctx_st *&encrypt, evp_cipher_ctx_st *&decrypt)
{
  if (!(encrypt = EVP_CIPHER_CTX_new()) || !(decrypt = EVP_CIPHER_CTX_new()))
    return false;
  if (1 != EVP_EncryptInit_ex(encrypt, EVP_aes_256_cfb1(), NULL, encrypt_key, encrypt_key+key_size) ||
      1 != EVP_DecryptInit_ex(decrypt, EVP_aes_256_cfb1(), NULL, decrypt_key, decrypt_key+key_size))
    return false;
  return true;
}

static bool receive_encryption(intptr_t sock, const uint8_t otp_page[key_plus_iv_size], EVP_CIPHER_CTX *&encrypt, EVP_CIPHER_CTX *&decrypt)
{
  EVP_CIPHER_CTX *decryptKeysCipher = EVP_CIPHER_CTX_new();
  if (1 != EVP_DecryptInit_ex(decryptKeysCipher, EVP_aes_256_cfb1(), NULL, otp_page, otp_page+key_size))
    return false;
  uint8_t encrypted_encrypt_key[key_plus_iv_size], encrypted_decrypt_key[key_plus_iv_size];
  memset(encrypted_encrypt_key, 0, sizeof(encrypted_encrypt_key)); memset(encrypted_decrypt_key, 0, sizeof(encrypted_decrypt_key));
  //server sends it's encryption key first, which is our decryption key; and it's decryption second, it is our encryption key
  if (raw_recv_exact(sock, encrypted_decrypt_key, sizeof(encrypted_decrypt_key)) <= 0 ||
      raw_recv_exact(sock, encrypted_encrypt_key, sizeof(encrypted_encrypt_key)) <= 0)
  {
    EVP_CIPHER_CTX_free(decryptKeysCipher);
    return false;
  }
  uint8_t encrypt_key[key_plus_iv_size], decrypt_key[key_plus_iv_size];
  memset(encrypt_key, 0, sizeof(encrypt_key)); memset(decrypt_key, 0, sizeof(decrypt_key));
  int outLen;
  if (1 != EVP_DecryptUpdate(decryptKeysCipher, decrypt_key, &outLen, encrypted_decrypt_key, sizeof(encrypted_decrypt_key)) ||
      outLen != sizeof(decrypt_key))
  {
    memset(encrypted_decrypt_key, 0, key_plus_iv_size);
    memset(encrypt_key, 0, key_plus_iv_size);
    EVP_CIPHER_CTX_free(decryptKeysCipher);
    return false;
  }
  memset(encrypted_decrypt_key, 0, key_plus_iv_size);
  if (1 != EVP_DecryptUpdate(decryptKeysCipher, encrypt_key, &outLen, encrypted_encrypt_key, sizeof(encrypted_encrypt_key)) ||
      outLen != sizeof(encrypt_key))
  {
    memset(encrypted_encrypt_key, 0, key_plus_iv_size);
    EVP_CIPHER_CTX_free(decryptKeysCipher);
    return false;
  }
  memset(encrypted_encrypt_key, 0, key_plus_iv_size);
  EVP_CIPHER_CTX_free(decryptKeysCipher);

  const bool ret = create_encryption_pair(encrypt_key, decrypt_key, encrypt, decrypt);
  memset(encrypt_key, 0, key_plus_iv_size);
  memset(decrypt_key, 0, key_plus_iv_size);
  return ret;
}

BlobSocket connect_to_server_blob_socket_no_auth(intptr_t raw_socket) {
  return BlobSocket{uintptr_t(raw_socket), nullptr, nullptr};
}

BlobSocket connect_to_server_blob_socket(intptr_t raw_socket, const uint8_t otp_page[key_plus_iv_size])
{
  if (raw_socket < 0)
    return BlobSocket{};
  evp_cipher_ctx_st *encryption = nullptr, *decryption = nullptr;

  if (!receive_encryption(raw_socket, otp_page, encryption, decryption))
  {
    if (encryption)
    {
      EVP_CIPHER_CTX_free(encryption);
    }
    if (decryption)
    {
      EVP_CIPHER_CTX_free(decryption);
    }
    raw_close_socket(raw_socket);
    return BlobSocket{};
  }
  return BlobSocket{uintptr_t(raw_socket), encryption, decryption};
}

//as server decrypt is same as client encrypt, we swap args
static bool send_encryption(intptr_t sock, const uint8_t otp_page[key_plus_iv_size], EVP_CIPHER_CTX *&encrypt, EVP_CIPHER_CTX *&decrypt)
{
  EVP_CIPHER_CTX *encryptKeysCipher= EVP_CIPHER_CTX_new();
  if (1 != EVP_EncryptInit_ex(encryptKeysCipher, EVP_aes_256_cfb1(), NULL, otp_page, otp_page+key_size))
    return false;

  //generate random keys
  uint8_t encrypt_key[key_plus_iv_size], decrypt_key[key_plus_iv_size];
  memset(encrypt_key, 0, sizeof(encrypt_key));memset(decrypt_key, 0, sizeof(decrypt_key));
  if (1 != RAND_bytes(encrypt_key, sizeof(encrypt_key)) || 1 != RAND_bytes(decrypt_key, sizeof(decrypt_key)))
  {
    EVP_CIPHER_CTX_free(encryptKeysCipher);
    memset(encrypt_key, 0, key_plus_iv_size);
    memset(decrypt_key, 0, key_plus_iv_size);
    return false;
  }
  if (!create_encryption_pair(encrypt_key, decrypt_key, encrypt, decrypt))
  {
    EVP_CIPHER_CTX_free(encryptKeysCipher);
    memset(encrypt_key, 0, key_plus_iv_size);
    memset(decrypt_key, 0, key_plus_iv_size);
    return false;
  }

  //encrypt random keys
  uint8_t encrypted_encrypt_key[key_plus_iv_size], encrypted_decrypt_key[key_plus_iv_size];
  memset(encrypted_encrypt_key, 0, sizeof(encrypted_encrypt_key)); memset(encrypted_decrypt_key, 0, sizeof(encrypted_decrypt_key));
  int outLen;
  if (1 != EVP_EncryptUpdate(encryptKeysCipher, encrypted_encrypt_key, &outLen, encrypt_key, sizeof(encrypt_key)) ||
      outLen != sizeof(encrypt_key))
  {
    memset(encrypted_encrypt_key, 0, key_plus_iv_size);
    memset(encrypt_key, 0, key_plus_iv_size);
    memset(decrypt_key, 0, key_plus_iv_size);
    EVP_CIPHER_CTX_free(encryptKeysCipher);
    return false;
  }
  memset(encrypt_key, 0, key_plus_iv_size);
  if (1 != EVP_EncryptUpdate(encryptKeysCipher, encrypted_decrypt_key, &outLen, decrypt_key, sizeof(decrypt_key)) ||
      outLen != sizeof(decrypt_key))
  {
    memset(encrypted_decrypt_key, 0, key_plus_iv_size);
    memset(encrypt_key, 0, key_plus_iv_size);
    memset(decrypt_key, 0, key_plus_iv_size);
    EVP_CIPHER_CTX_free(encryptKeysCipher);
    return false;
  }
  memset(decrypt_key, 0, key_plus_iv_size);
  EVP_CIPHER_CTX_free(encryptKeysCipher);

  //send encrypted keys to client.
  //client has same shared otp_page, so it will be able to decode it
  bool ret = (raw_send_exact(sock, encrypted_encrypt_key, sizeof(encrypted_encrypt_key))==sizeof(encrypted_encrypt_key) &&
              raw_send_exact(sock, encrypted_decrypt_key, sizeof(encrypted_encrypt_key))==sizeof(encrypted_encrypt_key));
  memset(encrypted_decrypt_key, 0, key_plus_iv_size);
  memset(encrypted_encrypt_key, 0, key_plus_iv_size);
  return ret;
}

BlobSocket connect_to_client_blob_socket_no_auth(intptr_t raw_socket) {
  return BlobSocket{uintptr_t(raw_socket), nullptr, nullptr};
}

BlobSocket connect_to_client_blob_socket(intptr_t raw_socket, uint8_t otp_page[key_plus_iv_size])
{
  if (raw_socket < 0)
    return BlobSocket{};
  evp_cipher_ctx_st *encryption = nullptr, *decryption = nullptr;

  if (!send_encryption(raw_socket, otp_page, encryption, decryption))
  {
    memset(otp_page, 0, key_plus_iv_size);
    if (encryption)
    {
      EVP_CIPHER_CTX_free(encryption);
    }
    if (decryption)
    {
      EVP_CIPHER_CTX_free(decryption);
    }
    raw_close_socket(raw_socket);
    return BlobSocket{};
  }
  memset(otp_page, 0, key_plus_iv_size);
  return BlobSocket{uintptr_t(raw_socket), encryption, decryption};
}

uint64_t blob_get_timestamp()
{
  //theoretically, it is stil vulnerable to 2038year problem. However, it can be easily fixed _here_ if needed (it is the only place)
  // in same time, many timestamps in many systems are at least valid till 2106year. I will be dead by that time, so please fix yourself
  return (uint64_t)time(NULL);
}

bool blob_is_valid_timestamp(uint64_t timestamp)
{
  const uint64_t myTime = blob_get_timestamp();
  return timestamp <= myTime + timestamp_valid_for_seconds && timestamp + timestamp_valid_for_seconds >= myTime;
}

// just sha384 from page number & shared secret
bool blob_gen_totp_secret(unsigned char generated_totp[key_plus_iv_size], const unsigned char *shared_secret, const uint32_t shared_secret_len, uint64_t page)
{
  if (shared_secret_len < minimum_shared_secret_length)
    return false;
  unsigned len = 0;
  unsigned char md[EVP_MAX_MD_SIZE];
  //EVP_sha384 is enough as it returns key_plus_iv_size
  if ( HMAC(EVP_sha384(), shared_secret, shared_secret_len, (const uint8_t*) &page, sizeof(page), md, &len) != md || len < key_plus_iv_size)
    return false;
  memcpy(generated_totp, md, key_plus_iv_size);
  return true;
}

static uint64_t blob_get_otp_page_from_seconds(const uint64_t ctime)
{
  static constexpr uint64_t seconds_per_otp_page = 131;//131 seconds per one otp page, this is prime number to mangle bits
  return ctime/seconds_per_otp_page;
}

uint64_t blob_get_otp_page()
{
  return blob_get_otp_page_from_seconds(blob_get_timestamp());
}

bool blob_is_valid_otp_page(uint64_t page)
{
  const uint64_t ctime = blob_get_timestamp();
  return page >= blob_get_otp_page_from_seconds(ctime - past_otp_page_valid_for_seconds) &&
         page <= blob_get_otp_page_from_seconds(ctime + timestamp_valid_for_seconds) ;
}
