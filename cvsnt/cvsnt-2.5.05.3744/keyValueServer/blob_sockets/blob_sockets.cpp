#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include "../include/blobs_encryption.h"
#include "../include/blob_sockets.h"
#include "../include/blob_raw_sockets.h"
#include <openssl/rand.h>


//we are using CTR mode, which is streaming

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
    blob_socket.encrypt = NULL;
  }
  if (blob_socket.decrypt)
  {
    EVP_CIPHER_CTX_free(blob_socket.decrypt);
    blob_socket.decrypt = NULL;
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
  if (1 != EVP_DecryptUpdate(cipher, to, &outLen, from, encrypted_len) || size_t(outLen) > space_left || outLen > encrypted_len)
    return -1;
  return outLen;
}

static int encrypt_and_finalize(evp_cipher_ctx_st *cipher, unsigned char *to, size_t space_left, const unsigned char *from, int decrypted_len)
{
  int outLen;
  if (1 != EVP_EncryptUpdate(cipher, to, &outLen, from, decrypted_len) || size_t(outLen) > space_left || outLen > decrypted_len)
    return -1;
  return outLen;
}

int recv(BlobSocket &socket, void *buf_, int len)
{
  if (!socket.decrypt)
    return recv((SOCKET)socket.opaque, (char*)buf_, len, 0);
  unsigned char* buf = (unsigned char*)buf_;
  const int lenRead = len;
  int read = 0, decrypted = 0;
  for (int lenLeft = lenRead; lenLeft > 0;)
  {
    unsigned char decryptedBuf[32768];//decode by 32768
    const int curLen = size_t(lenLeft) < sizeof(decryptedBuf) ? lenLeft : (int)sizeof(decryptedBuf);
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
    const int curLen = size_t(lenLeft) > sizeof(encryptedBuf) ? lenLeft : (int)sizeof(encryptedBuf);
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
  if (encrypted != len)
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

static bool create_encryption_pair(const uint8_t encrypt_key[key_plus_iv_size], const uint8_t decrypt_key[key_plus_iv_size], evp_cipher_ctx_st *&encrypt, evp_cipher_ctx_st *&decrypt)
{
  if (!(encrypt = EVP_CIPHER_CTX_new()) || !(decrypt = EVP_CIPHER_CTX_new()))
    return false;
  if (1 != EVP_EncryptInit_ex(encrypt, EVP_aes_128_ctr(), NULL, encrypt_key, encrypt_key+key_size) ||
      1 != EVP_DecryptInit_ex(decrypt, EVP_aes_128_ctr(), NULL, decrypt_key, decrypt_key+key_size))
    return false;
  return true;
}

static EVP_CIPHER_CTX *init_cipher_from_otp(const uint8_t otp_page[otp_page_size], bool enc)
{
  EVP_CIPHER_CTX *cipher = EVP_CIPHER_CTX_new();
  uint8_t iv[full_iv_size]; memset(iv, 0, sizeof(iv));
  memcpy(iv, otp_page+key_size, 16); iv[full_iv_size-1] = 0;//guarantee that we have one block to decode
  if (1 != (enc ? EVP_EncryptInit_ex(cipher, EVP_aes_128_ctr(), NULL, otp_page, iv) :
                  EVP_DecryptInit_ex(cipher, EVP_aes_128_ctr(), NULL, otp_page, iv)))
  {
    memset(iv, 0, sizeof(iv));
    EVP_CIPHER_CTX_free(cipher);
    return nullptr;
  }
  memset(iv, 0, sizeof(iv));
  return cipher;
}

//generates Diffie Hellman keys, https://en.wikipedia.org/wiki/Diffie%E2%80%93Hellman_key_exchange

static EVP_PKEY * get_peerkey(const unsigned char * buffer, size_t buffer_len)
{
  EC_KEY *tempEcKey = NULL;
  EVP_PKEY *peerkey = NULL;
  bool ret = true;
  // change this if another curve is required
  tempEcKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if(tempEcKey == NULL) {ret = false; goto end;}

  if (EC_KEY_oct2key(tempEcKey, buffer, buffer_len, NULL) != 1)  {ret = false; goto end;}

  if(EC_KEY_check_key(tempEcKey) != 1) {ret = false; goto end;}

  peerkey = EVP_PKEY_new();
  if(peerkey == NULL) {ret = false; goto end;}

  if(EVP_PKEY_assign_EC_KEY(peerkey, tempEcKey)!= 1) {ret = false; goto end;}
end:;
  if (!ret)
  {
    EC_KEY_free(tempEcKey);
    EVP_PKEY_free(peerkey);
    peerkey = NULL;
  }
  return peerkey;
}

static size_t get_buffer(EVP_PKEY * pkey, unsigned char ** client_pub_key)
{
  EC_KEY *tempEcKey = EVP_PKEY_get0_EC_KEY(pkey);
  if (tempEcKey == NULL) return false;
  //write in the buffer
  return EC_KEY_key2buf(tempEcKey, POINT_CONVERSION_COMPRESSED, client_pub_key, NULL);
}

/* Never use a derived secret directly. */
template <class CB>
static bool ecdh(uint8_t *secret, size_t &secret_len, CB get_peer_key_data)
{
  EVP_PKEY_CTX *pctx = NULL, *kctx = NULL;
  EVP_PKEY_CTX *ctx = NULL;
  EVP_PKEY *pkey = NULL, *peerkey = NULL, *params = NULL;
  uint8_t *pubKey = nullptr;
  const uint8_t* peerkey_data = NULL;
  bool ret = true;
  size_t pubKeyLen = 0;
  memset(secret, 0, secret_len);

  /* Create the context for parameter generation */
  if(NULL == (pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL))) {ret = false; goto end;}

  /* Initialise the parameter generation */
  if(1 != EVP_PKEY_paramgen_init(pctx)) {ret = false; goto end;}

  /* We're going to use the ANSI X9.62 Prime 256v1 curve */
  if(1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1)) {ret = false; goto end;}

  /* Create the parameter object params */
  if (!EVP_PKEY_paramgen(pctx, &params)) {ret = false; goto end;}

  /* Create the context for the key generation */
  if(NULL == (kctx = EVP_PKEY_CTX_new(params, NULL))) {ret = false; goto end;}

  /* Generate the key */
  if(1 != EVP_PKEY_keygen_init(kctx)) {ret = false; goto end;}
  if (1 != EVP_PKEY_keygen(kctx, &pkey)) {ret = false; goto end;}

  /* Get the peer's public key, and provide the peer with our public key -
   * how this is done will be specific to your circumstances */
  pubKeyLen = get_buffer(pkey, &pubKey);
  if (pubKeyLen == 0) { ret = false; goto end; }
  peerkey_data = get_peer_key_data(pubKey, (uint32_t)pubKeyLen);
  if (peerkey_data == NULL) { ret = false; goto end; }
  peerkey = get_peerkey(peerkey_data, pubKeyLen);

  /* Create the context for the shared secret derivation */
  if(NULL == (ctx = EVP_PKEY_CTX_new(pkey, NULL))) {ret = false; goto end;}

  /* Initialise */
  if(1 != EVP_PKEY_derive_init(ctx)) {ret = false; goto end;}

  /* Provide the peer public key */
  if(1 != EVP_PKEY_derive_set_peer(ctx, peerkey)) {ret = false; goto end;}

  /* Derive the shared secret */
  if(1 != (EVP_PKEY_derive(ctx, secret, &secret_len))) {ret = false; goto end;}

end:;
  OPENSSL_free(pubKey);
  EVP_PKEY_CTX_free(ctx);
  EVP_PKEY_free(peerkey);
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(kctx);
  EVP_PKEY_free(params);
  EVP_PKEY_CTX_free(pctx);
  return ret;
}

static bool exchange_session_keys(intptr_t raw_socket, const uint8_t otp_page[otp_page_size], evp_cipher_ctx_st *&encrypt, evp_cipher_ctx_st *&decrypt, bool is_server)
{
  static constexpr size_t pub_key_data_len = 33;
  static constexpr size_t random_data_len = 8;
  static constexpr size_t max_auth_data_len = pub_key_data_len+random_data_len;
  uint8_t ourDhAuthData[max_auth_data_len], otherDhAuthData[max_auth_data_len];
  memset(ourDhAuthData, 0, sizeof(ourDhAuthData));
  memset(otherDhAuthData, 0, sizeof(otherDhAuthData));
  uint8_t *ourRandom = ourDhAuthData + pub_key_data_len;
  if (1 != RAND_bytes(ourRandom, random_data_len))
    return false;

  constexpr size_t max_secret_len = 32;
  uint8_t secretData[max_secret_len + random_data_len*2];
  size_t secret_len = max_secret_len;
  memset(secretData, 0, sizeof(secretData));
  if (!ecdh(secretData, secret_len, [&](const uint8_t *ourData, uint32_t pub_len)->const uint8_t*
      {
          if (pub_len != pub_key_data_len)
              return nullptr;
          memcpy(ourDhAuthData, ourData, pub_len);
          EVP_CIPHER_CTX *enc = init_cipher_from_otp(otp_page, true);
          uint8_t encrypted[sizeof(ourDhAuthData)];
          int outLen = 0;
          if (1 != EVP_EncryptUpdate(enc, encrypted, &outLen, ourDhAuthData, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
            return nullptr;
          EVP_CIPHER_CTX_free(enc); enc = NULL;

          if (raw_send_exact(raw_socket, encrypted, sizeof(encrypted)) <= 0)
          {
              memset(encrypted, 0, sizeof(encrypted));
              memset(ourDhAuthData, 0, sizeof(ourDhAuthData));
              return nullptr;
          }
          memset(encrypted, 0, sizeof(encrypted));

          if (raw_recv_exact(raw_socket, encrypted, sizeof(encrypted)) <= 0)
          {
              memset(ourDhAuthData, 0, sizeof(ourDhAuthData));
              return nullptr;
          }
          EVP_CIPHER_CTX *dec = init_cipher_from_otp(otp_page, false);
          if (1 != EVP_DecryptUpdate(dec, otherDhAuthData, &outLen, encrypted, sizeof(ourDhAuthData)) || outLen != sizeof(encrypted))
            return nullptr;
          memset(encrypted, 0, sizeof(encrypted));
          EVP_CIPHER_CTX_free(enc); enc = NULL;
          return otherDhAuthData;
      }
      ))
  {
    //blob_logmessage(LOG_ERROR, "Can't generate DH keys.");
    return false;
  }
  const uint8_t* otherRandom = otherDhAuthData + pub_key_data_len;
  //add randoms in same order
  memcpy(secretData + max_secret_len, is_server ? ourRandom : otherRandom, random_data_len);
  memcpy(secretData + max_secret_len + random_data_len, is_server ? otherRandom : ourRandom, random_data_len);
  memset(ourDhAuthData, 0, sizeof(ourDhAuthData));
  memset(otherDhAuthData, 0, sizeof(otherDhAuthData));
  uint8_t pair_keys[sent_key_size*2];
  memset(pair_keys,0,sizeof(pair_keys));
  uint32_t len = 0;
  //EVP_sha384 returns 48 bytes, sent_key_size*2
  if ( HMAC(EVP_sha384(), secretData, (int)secret_len, otp_page, otp_page_size, pair_keys, &len) != pair_keys || len != sizeof(pair_keys))
  {
    memset(secretData, 0, sizeof(secretData));
    return false;
  }
  //encryption for server is decryption for client
  uint8_t pair[key_plus_iv_size*2]; memset(pair,0,sizeof(pair));
  memcpy(pair, is_server ? pair_keys : pair_keys+sent_key_size, sent_key_size);
  memcpy(pair+key_plus_iv_size, !is_server ? pair_keys : pair_keys+sent_key_size, sent_key_size);
  memset(pair_keys, 0, sizeof(pair_keys));
  const bool ret = create_encryption_pair(pair, pair+key_plus_iv_size, encrypt, decrypt);
  memset(pair, 0, sizeof(pair));
  return ret;
}

BlobSocket connect_to_server_blob_socket_no_auth(intptr_t raw_socket) {
  return BlobSocket{uintptr_t(raw_socket), nullptr, nullptr};
}

static BlobSocket encrypted_socket(intptr_t raw_socket, const uint8_t otp_page[otp_page_size], bool is_server)
{
  if (raw_socket < 0)
    return BlobSocket{};
  evp_cipher_ctx_st *encryption = nullptr, *decryption = nullptr;
  if (!exchange_session_keys(raw_socket, otp_page, encryption, decryption, is_server))
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

BlobSocket connect_to_server_blob_socket(intptr_t raw_socket, const uint8_t otp_page[otp_page_size])
{
  return encrypted_socket(raw_socket, otp_page, false);
}

BlobSocket connect_to_client_blob_socket_no_auth(intptr_t raw_socket) {
  return BlobSocket{uintptr_t(raw_socket), nullptr, nullptr};
}

BlobSocket connect_to_client_blob_socket(intptr_t raw_socket, uint8_t otp_page[otp_page_size])
{
  auto ret = encrypted_socket(raw_socket, otp_page, true);
  memset(otp_page, 0, otp_page_size);
  return ret;
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
bool blob_gen_totp_secret(unsigned char generated_totp[otp_page_size], const unsigned char *shared_secret, const uint32_t shared_secret_len, uint64_t page)
{
  if (shared_secret_len < minimum_shared_secret_length)
    return false;
  unsigned len = 0;
  unsigned char md[EVP_MAX_MD_SIZE];
  //EVP_sha384 is enough as it returns 48
  if ( HMAC(EVP_sha384(), shared_secret, shared_secret_len, (const uint8_t*) &page, sizeof(page), md, &len) != md || len < otp_page_size)
    return false;
  memcpy(generated_totp, md, otp_page_size);
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
