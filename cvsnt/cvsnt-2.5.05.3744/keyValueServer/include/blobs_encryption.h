#pragma once
#include <stdint.h>
//Blob socket encryption is implemented with symmetrical AES-256
//
//After initial greetings. Client & serer exchanges version's. New version (2+) client to new version server will require to send pageNo from OTP, based on time on a client.
//
//Old version (1) client can work only with Local or Public Encryption servers (or old servers).
//New version server will always reply to vew version client with encryption keys.
//If so, client alwys send to server otp page no, immediately after it's version.
//CAFS server than send (randomly generated) pair of keys for encryption, this pair IS also _encrypted_ with OTP key generated with pageNo.
//OTP key is generated from preshared secret & otp page number.
//Since then they both start talking using this pair of keys, OTP is no longer needed.
//using those keys, client will send it's time, and server will respond with AUTH answer
//  HAVE - we will be keeping encryption
//  NONE - remove encryption. Is to save some peformance in private network. It is rather safe, as we passed authentication and we are both in same private network (if it is office, MITM attack in the office on blobs is both unlikely and unproductive (client verifies hash)
//  ERIO - time is ivalid. we don't want to work with you as it may be traffic replcation
//Both CAFS client and CAFS server have to have same page from One-time pad - CAFS client sends to cafs server just page no,
// but it has to have OTP page to decrypt pair of keys.
//They can share same shared secret for OTP, but it's enough to only have one page.
//In CVS we rely on a sharing just generated _page_ from OTP, which CVS client obtains from CVS server.
//Since CVS client and server already talked via encrypted channel, it is most secure way, we don't have to share a secret, and CVS requires authorization to send OTP.

//Our CAFS server validates that page no isn't too strange (outdated or from future).
//
//If CAFS server demands encryption, it will send data encrypted, denying old clients.
//For encypting CAFS server, first packet from client after greeting IS OTP page.
//If CAFS client _demands_ encryption, it can just refuse to work with server with unencrypted traffic
//(and when used in CVS client it will refuse, if it is not private IP, to avoid MITM attack).

//for Server:
//Local - will accept only private network and the only way we can run without encryption_secret. This is for local proxys.
//Public - will accept both old and new versions clients, and for new version clients will not encrypt traffic for private network address. Authentication is still performed.
//All - will accept only new versions clients, and will encrypt traffic for both public & private network address. Authentication is also performed.

enum class CafsServerEncryption {Local, Public, All};

//for Client:
//AllowNoAuthPrivate  - will accept old servers in private network
//RequiresAuth - will accept only new versions servers with authentication
enum class CafsClientAuthentication {AllowNoAuthPrivate, RequiresAuth};

//we use AES256, so our key size is 32. and IV size is 16
static constexpr size_t key_size = 32, iv_size = 16;
static constexpr size_t key_plus_iv_size = key_size+iv_size;

//for security reasons, minimum shared secret length is 16
static constexpr size_t minimum_shared_secret_length = 16;

static constexpr uint64_t past_otp_page_valid_for_seconds = 8*60*60;//if we connected 8 hour ago, it is still valid
static constexpr uint64_t timestamp_valid_for_seconds = 10*60;//allow 10 minutes time difference (wrong clock)

extern uint64_t blob_get_timestamp();
extern bool blob_is_valid_timestamp(uint64_t timestamp);
extern bool blob_gen_totp_secret(unsigned char generated_totp[key_plus_iv_size], const unsigned char *shared_secret, const uint32_t shared_secret_len, uint64_t page);
extern uint64_t blob_get_otp_page();
extern bool blob_is_valid_otp_page(uint64_t page);
extern void blob_sleep_for_usec(uint64_t usec);

