#pragma once

#include <map>
#include "conn.hh"
#include "reader.hh"
#include "writer.hh"

namespace kvz_rtp {
    class context {

    public:
        context();
        ~context();

        /* Start listening to incoming RTP packets form src_addr:src_port
         *
         * Read packets are stored in a ring buffer which can be read by
         * calling kvz_rtp::reader::pull_frame() */
        kvz_rtp::reader *create_reader(std::string src_addr, int src_port);
        kvz_rtp::reader *create_reader(std::string src_addr, int src_port, rtp_format_t fmt);

        /* Open connection for writing RTP packets to dst_addr:dst_port
         *
         * Packets can be sent by calling kvz_rtp::writer::push_frame() */
        kvz_rtp::writer *create_writer(std::string dst_addr, int dst_port);
        kvz_rtp::writer *create_writer(std::string dst_addr, int dst_port, rtp_format_t fmt);
        kvz_rtp::writer *create_writer(std::string dst_addr, int dst_port, int src_port);
        kvz_rtp::writer *create_writer(std::string dst_addr, int dst_port, int src_port, rtp_format_t fmt);

    private:
        std::map<uint32_t, connection *> conns_;
    };
};
