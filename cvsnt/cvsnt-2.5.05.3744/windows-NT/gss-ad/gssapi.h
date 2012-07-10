/*
	GSSAPI wrapper for Active Directory
    Copyright (C) 2003-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef GSSAPI__H
#define GSSAPI__H

#define SECURITY_WIN32
#include <security.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int OM_uint32;
typedef int OM_int32;

typedef CtxtHandle gss_ctx_id_t;
typedef CredHandle gss_cred_id_t;
typedef SEC_CHAR* gss_name_t;

typedef unsigned int gss_OID_set;
typedef void* gss_channel_bindings_t;

typedef struct
{
	void *value;
	size_t length;
} gss_buffer_desc, *gss_buffer_t;

typedef enum
{
	GSS_C_NT_HOSTBASED_SERVICE
} gss_OID;

typedef enum
{
	GSS_C_QOP_DEFAULT
} gss_qop_t;

enum 
{
	GSS_C_DELEG_FLAG = ISC_REQ_DELEGATE, //The server is allowed to impersonate the client. 
	GSS_C_MUTUAL_FLAG = ISC_REQ_MUTUAL_AUTH, //Client and server will be authenticated. 
	GSS_C_REPLAY_FLAG = ISC_REQ_REPLAY_DETECT, // Detect replayed packets.
	GSS_C_SEQUENCE_FLAG = ISC_REQ_SEQUENCE_DETECT, // Detect messages received out of sequence.
	GSS_C_CONF_FLAG = ISC_REQ_CONFIDENTIALITY, // Encrypt messages.
	GSS_C_INTEG_FLAG = ISC_REQ_INTEGRITY, // Sign messages and verify signatures. 
};

static const gss_ctx_id_t GSS_C_NO_CONTEXT = {0};
static const gss_cred_id_t GSS_C_NO_CREDENTIAL = {0};
static const gss_buffer_desc GSS_C_EMPTY_BUFFER = {0};

#define GSS_C_NO_BUFFER   (gss_buffer_t)NULL
#define GSS_C_NULL_OID   (gss_OID)NULL
#define GSS_C_NULL_OID_SET   (gss_OID_set)NULL
#define GSS_C_NO_BUFFER   (gss_buffer_t)NULL
#define GSS_C_QOP_DEFAULT   0  
#define GSS_C_INDEFINITE   0xffffffff  

#define GSS_S_COMPLETE			SEC_E_OK
#define GSS_S_CONTINUE_NEEDED   SEC_I_CONTINUE_NEEDED
#define GSS_S_ERROR				0xFFFFFFFF

#define GSS_C_BOTH		SECPKG_CRED_BOTH
#define GSS_C_INITIATE	SECPKG_CRED_OUTBOUND 
#define GSS_C_ACCEPT	SECPKG_CRED_INBOUND

#define GSS_C_GSS_CODE 1
#define GSS_C_MECH_CODE 2

OM_uint32 gss_import_name (
	    OM_uint32 *minor_status,
		gss_buffer_t input_name_buffer,
		gss_OID input_name_type,
		gss_name_t* output_name);

OM_uint32  gss_init_sec_context (
        OM_uint32 *minor_status,
        gss_cred_id_t claimant_cred_handle,
        gss_ctx_id_t *context_handle,
        gss_name_t target_name,
        gss_OID mech_type,
        int req_flags,
        int time_req,
        gss_channel_bindings_t input_channel_bindings,
        gss_buffer_t input_token,
        gss_OID *actual_mech_types,
        gss_buffer_t output_token,
        int *ret_flags,
        OM_int32 *time_rec);

OM_uint32  gss_acquire_cred (
        OM_uint32 *minor_status,
        gss_name_t desired_name,
        OM_uint32 time_req,
        gss_OID_set desired_mechs,
        int cred_usage,
        gss_cred_id_t *output_cred_handle,
        gss_OID_set *actual_mechs,
        OM_int32 *time_rec);

OM_uint32  gss_release_name (
        OM_uint32 *minor_status,
        gss_name_t *name);

OM_uint32  gss_accept_sec_context (
        OM_uint32 *minor_status,
        gss_ctx_id_t *context_handle,
        gss_cred_id_t verifier_cred_handle,
        gss_buffer_t input_token_buffer,
        gss_channel_bindings_t input_chan_bindings,
        gss_name_t *src_name,
        gss_OID *actual_mech_type,
        gss_buffer_t output_token,
        OM_uint32 *ret_flags,
        OM_uint32 *time_rec,
        gss_cred_id_t *delegated_cred_handle);

OM_uint32 gss_display_name (
		OM_uint32 *minor_status,
		gss_name_t input_name,
		gss_buffer_t output_name_buffer,
		gss_OID *output_name_type);

OM_uint32 gss_display_status (
        OM_uint32 *minor_status,
        int status_value,
        int status_type,
        gss_OID mech_type,
        OM_uint32 *message_context,
        gss_buffer_t status_string);

OM_uint32 gss_unwrap (
		OM_uint32 * minor_status,
		gss_ctx_id_t context_handle,
		gss_buffer_t input_message_buffer,
		gss_buffer_t output_message_buffer,
		int * conf_state,
		gss_qop_t * qop_state);

OM_uint32 gss_wrap (
		OM_uint32 *minor_status,
		gss_ctx_id_t context_handle,
		int conf_req_flag,
		gss_qop_t qop_req,
		gss_buffer_t input_message_buffer,
		int *conf_state,
		gss_buffer_t output_message_buffer);

OM_uint32 gss_release_buffer (
        OM_uint32 *minor_status,
        gss_buffer_t buffer);

/* Specific to this library - register SPN with directory server */
OM_uint32 gss_ad_register_spn(OM_uint32 *minor_status, gss_name_t service_name);
OM_uint32 gss_ad_unregister_spn(OM_uint32 *minor_status, gss_name_t service_name);

#ifdef __cplusplus
}
#endif

#endif // GSSAPI__H