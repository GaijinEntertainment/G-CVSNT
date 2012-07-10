#ifndef foodnshfoo
#define foodnshfoo

/* $Id: dns.h,v 1.1.2.3 2005/10/14 16:23:53 tmh Exp $ */

/***
  This file is part of nss-mdns.
 
  nss-mdns is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  nss-mdns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with nss-mdns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <sys/types.h>
#ifndef _WIN32
#include <stdarg.h> // HPUX needs this in inttypes.h...
#include <inttypes.h>
#else
#include "win32/inttypes.h"
#endif
#include <stddef.h>

struct dns_packet {
    size_t size, rindex;
    uint8_t data[9000];
};

struct ipv4_address_struct_t;
struct ipv6_address_struct_t;

struct dns_packet* dns_packet_new(void);
void dns_packet_free(struct dns_packet *p);
void dns_packet_set_field(struct dns_packet *p, unsigned index, uint16_t v);
uint16_t dns_packet_get_field(struct dns_packet *p, unsigned index);

uint8_t *dns_packet_append_uint16(struct dns_packet *p, uint16_t v);
uint8_t *dns_packet_append_uint32(struct dns_packet *p, uint32_t v);
uint8_t *dns_packet_append_ipv4(struct dns_packet *p, struct ipv4_address_struct_t *v);
uint8_t *dns_packet_append_ipv6(struct dns_packet *p, struct ipv6_address_struct_t *v);
uint8_t *dns_packet_append_text(struct dns_packet *p, const char *name);
uint8_t *dns_packet_append_name(struct dns_packet *p, const char *name);
uint8_t *dns_packet_append_name_compressed(struct dns_packet *p, const char *name, uint8_t *prev);
uint8_t *dns_packet_extend(struct dns_packet *p, size_t l);
int dns_packet_check_valid_response(struct dns_packet *p);
int dns_packet_check_valid_request(struct dns_packet *p);
int dns_packet_check_valid(struct dns_packet *p);

int dns_packet_consume_name(struct dns_packet *p, char *ret_name, size_t l);
int dns_packet_consume_text(struct dns_packet *p, char *ret_name, size_t l);
int dns_packet_consume_uint16(struct dns_packet *p, uint16_t *ret_v);
int dns_packet_consume_uint32(struct dns_packet *p, uint32_t *ret_v);
int dns_packet_consume_bytes(struct dns_packet *p, void *ret_data, size_t l);
int dns_packet_consume_seek(struct dns_packet *p, size_t length);

#define DNS_TYPE_INVALID  0     // Cookie
#define DNS_TYPE_A        1     // Host address
#define DNS_TYPE_NS       2     // Authoritative server
#define DNS_TYPE_MD       3     // Mail destination
#define DNS_TYPE_MF       4     // Mail forwarder
#define DNS_TYPE_CNAME    5     // Canonical name
#define DNS_TYPE_SOA      6     // Start of authority zone
#define DNS_TYPE_MB       7     // Mailbox domain name
#define DNS_TYPE_MG       8     // Mail group member
#define DNS_TYPE_MR       9     // Mail rename name
#define DNS_TYPE_NULL     10    // Null resource record
#define DNS_TYPE_WKS      11    // Well known service
#define DNS_TYPE_PTR      12    // Domain name pointer
#define DNS_TYPE_HINFO    13    // Host information
#define DNS_TYPE_MINFO    14    // Mailbox information
#define DNS_TYPE_MX       15    // Mail routing information
#define DNS_TYPE_TXT      16    // Text strings
#define DNS_TYPE_RP       17    // Responsible person
#define DNS_TYPE_AFSDB    18    // AFS cell database
#define DNS_TYPE_X25      19    // X_25 calling address
#define DNS_TYPE_ISDN     20    // ISDN calling address
#define DNS_TYPE_RT       21    // Router
#define DNS_TYPE_NSAP     22    // NSAP address
#define DNS_TYPE_NSAP_PTR 23    // Reverse NSAP lookup (deprecated)
#define DNS_TYPE_SIG      24    // Security signature
#define DNS_TYPE_KEY      25    // Security key
#define DNS_TYPE_PX       26    // X.400 mail mapping
#define DNS_TYPE_GPOS     27    // Geographical position (withdrawn)
#define DNS_TYPE_AAAA     28    // Ip6 Address
#define DNS_TYPE_LOC      29    // Location Information
#define DNS_TYPE_NXT      30    // Next domain (security)
#define DNS_TYPE_EID      31    // Endpoint identifier
#define DNS_TYPE_NIMLOC   32    // Nimrod Locator
#define DNS_TYPE_SRV      33    // Server Selection
#define DNS_TYPE_ATMA     34    // ATM Address
#define DNS_TYPE_NAPTR    35    // Naming Authority PoinTeR
#define DNS_TYPE_KX       36    // Key Exchange
#define DNS_TYPE_CERT     37    // Certification record
#define DNS_TYPE_A6       38    // IPv6 address (deprecates AAAA)
#define DNS_TYPE_DNAME    39    // Non-terminal DNAME (for IPv6)
#define DNS_TYPE_SINK     40    // Kitchen sink (experimentatl)
#define DNS_TYPE_OPT      41    // EDNS0 option (meta-RR)

#define DNS_TYPE_TSIG     250   // Transaction signature
#define DNS_TYPE_IXFR     251   // Incremental zone transfer
#define DNS_TYPE_AXFR     252   // Transfer zone of authority
#define DNS_TYPE_MAILB    253   // Transfer mailbox records
#define DNS_TYPE_MAILA    254   // Transfer mail agent records
#define DNS_TYPE_ANY      255   // Wildcard match

#define DNS_CLASS_INVALID  0    // Cookie
#define DNS_CLASS_IN       1    // Internet
#define DNS_CLASS_2        2    // Unallocated/unsupported
#define DNS_CLASS_CHAOS    3    // MIT Chaos-net
#define DNS_CLASS_HS       4    // MIT Hesiod

#define DNS_CLASS_NONE     254  // For prereq. sections in update request
#define DNS_CLASS_ANY      255  // Wildcard match

#define DNS_CLASS_FLUSH		0x8000 // Flush

#define DNS_FIELD_ID 0
#define DNS_FIELD_FLAGS 1
#define DNS_FIELD_QDCOUNT 2
#define DNS_FIELD_ANCOUNT 3
#define DNS_FIELD_NSCOUNT 4
#define DNS_FIELD_ARCOUNT 5

#define DNS_FLAG_QR (1 << 15)
#define DNS_FLAG_OPCODE (15 << 11)
#define DNS_FLAG_RCODE (15)

#define DNS_FLAGS(qr, opcode, aa, tc, rd, ra, z, ad, cd, rcode) \
        (((uint16_t) !!qr << 15) |  \
         ((uint16_t) (opcode & 15) << 11) | \
         ((uint16_t) !!aa << 10) | \
         ((uint16_t) !!tc << 9) | \
         ((uint16_t) !!rd << 8) | \
         ((uint16_t) !!ra << 7) | \
         ((uint16_t) !!ad << 5) | \
         ((uint16_t) !!cd << 4) | \
         ((uint16_t) (rcode & 15)))
         

#endif

