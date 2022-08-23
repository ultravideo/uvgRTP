#include <iostream>
#include <string>

#include <uvgrtp/lib.hh>
#include <uvgrtp/wrapper_c.hh>

void
uvgrtp_create_ctx(void** uvgrtp_context)
{
    uvgrtp::context* ctx= new uvgrtp::context();
    std::cout << ctx << std::endl;
    *uvgrtp_context = ctx;
}

void
uvgrtp_destroy_ctx(void* uvgrtp_context)
{
    if (uvgrtp_context){
        uvgrtp::context* uvg_ctx_ptr = (uvgrtp::context*)uvgrtp_context;
        delete uvg_ctx_ptr;
    }
}

void
uvgrtp_create_session(void* uvgrtp_context, void** uvgrtp_session, char* remote_address)
{
    std::cout << uvgrtp_context << std::endl;
    uvgrtp::context* uvg_ctx_ptr = (uvgrtp::context*)uvgrtp_context;
    uvgrtp::session *sess = uvg_ctx_ptr->create_session(remote_address);
    *uvgrtp_session = sess;
}

void
uvgrtp_destroy_session(void* uvgrtp_context, void* uvgrtp_session)
{
    if (uvgrtp_context){
        uvgrtp::context* uvg_ctx_ptr = (uvgrtp::context*)uvgrtp_context;
        if (uvgrtp_session){
           uvgrtp::session* uvg_sess_ptr = (uvgrtp::session*)uvgrtp_session;
           uvg_ctx_ptr->destroy_session(uvg_sess_ptr);
        }
    }
}

void
uvgrtp_create_stream(void* uvgrtp_session, void** uvgrtp_stream, uint16_t local_port, uint16_t remote_port, 
    int rce_flags)
{
    rtp_format_t fmt = RTP_FORMAT_H265;
    uvgrtp::session* uvg_sess_ptr = (uvgrtp::session*)uvgrtp_session;
    uvgrtp::media_stream *stream = uvg_sess_ptr->create_stream(local_port, remote_port, fmt, rce_flags);
    *uvgrtp_stream = stream;
}

void
uvgrtp_destroy_stream(void* uvgrtp_session, void* uvgrtp_stream)
{
    uvgrtp::session* uvg_sess_ptr = (uvgrtp::session*)uvgrtp_session;
    uvgrtp::media_stream* uvg_stream_ptr = (uvgrtp::media_stream*)uvgrtp_stream;
    uvg_sess_ptr->destroy_stream(uvg_stream_ptr);
}

void
uvgrtp_push_frame(void* uvgrtp_stream, uint8_t* data, size_t data_len, int rtp_flags)
{
    uvgrtp::media_stream* uvg_stream_ptr = (uvgrtp::media_stream*)uvgrtp_stream;
    uvg_stream_ptr->push_frame(data, data_len, rtp_flags);
}