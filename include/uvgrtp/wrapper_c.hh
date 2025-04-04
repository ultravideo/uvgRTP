#ifndef UVGRTP_H
#define UVGRTP_H

#include "uvgrtp/export.hh"

#ifdef __cplusplus
extern "C"
{
#endif

void UVGRTP_EXPORT uvgrtp_create_ctx(void** uvgrtp_context);

void UVGRTP_EXPORT uvgrtp_create_session(void* uvgrtp_context, void** uvgrtp_session, char* remote_address);

void UVGRTP_EXPORT uvgrtp_create_stream(void* uvgrtp_session, void** uvgrtp_stream, uint16_t local_port, uint16_t remote_port, int rce_flags);

void UVGRTP_EXPORT uvgrtp_destroy_ctx(void* uvgrtp_context);

void UVGRTP_EXPORT uvgrtp_destroy_session(void* uvgrtp_context, void* uvgrtp_session);

void UVGRTP_EXPORT uvgrtp_destroy_stream(void* uvgrtp_session, void* uvgrtp_stream);

void UVGRTP_EXPORT uvgrtp_push_frame(void* uvgrtp_stream, uint8_t* data, size_t data_len, int rtp_flags);

#ifdef __cplusplus
}
#endif

#endif //UVGRTP_H