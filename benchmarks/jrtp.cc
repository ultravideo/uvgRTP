#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpipv4address.h>
#include <jrtplib3/rtptimeutilities.h>
#include <jrtplib3/rtppacket.h>
#include <stdlib.h>
#include <iostream>
#include <chrono>
#include <cstring>

using namespace jrtplib;

extern void *get_mem(int argc, char **argv, size_t& len);
extern int get_next_frame_start(uint8_t *data, uint32_t offset, uint32_t data_len, uint8_t& start_len);

#define MAX_WRITE_SIZE 1385

int push_hevc_nal_unit(RTPSession& session, uint8_t *data, size_t data_len)
{
    uint8_t nalType  = (data[0] >> 1) & 0x3F;
    size_t data_left = data_len;
    size_t data_pos  = 0;
    int status       = 0;

    if (data_len < MAX_WRITE_SIZE) {
        if ((status = session.SendPacket(data, data_len)) < 0) {
            std::cerr << RTPGetErrorString(status) << std::endl;
            exit(-1);
        }
        return 0;
    }

    uint8_t buffer[2 + 1 + MAX_WRITE_SIZE];

    buffer[0]  = 49 << 1;           /* fragmentation unit */
    buffer[1]  = 1;                 /* TID */
    buffer[2] = (1 << 7) | nalType; /* Start bit + NAL type */

    data_pos   = 2;
    data_left -= 2;

    while (data_left > MAX_WRITE_SIZE) {
        memcpy(&buffer[3], &data[data_pos], MAX_WRITE_SIZE);

        if ((status = session.SendPacket(buffer, sizeof(buffer), 96, false, 0) < 0)) {
            std::cerr << RTPGetErrorString(status) << std::endl;
            exit(-1);
        }

        data_pos  += MAX_WRITE_SIZE;
        data_left -= MAX_WRITE_SIZE;

        /* Clear extra bits */
        buffer[2] = nalType;
    }
    buffer[2] |= (1 << 6); /* set E bit to signal end of data */

    memcpy(&buffer[3], &data[data_pos], data_left);

    if ((status = session.SendPacket(buffer, 2 + 1 + data_left, 96, true, 1) < 0)) {
        std::cerr << RTPGetErrorString(status) << std::endl;
        exit(-1);
    }
}

int push_hevc_chunk(RTPSession& s, uint8_t *data, size_t data_len)
{
    uint8_t start_len;
    int32_t prev_offset = 0;
    int offset = get_next_frame_start(data, 0, data_len, start_len);
    prev_offset = offset;

    while (offset != -1) {
        offset = get_next_frame_start(data, offset, data_len, start_len);

        if (offset > 4 && offset != -1) {
            push_hevc_nal_unit(s, &data[prev_offset], offset - prev_offset - start_len);
            prev_offset = offset;
        }
    }

    if (prev_offset == -1)
        prev_offset = 0;

    push_hevc_nal_unit(s, &data[prev_offset], data_len - prev_offset);
}

int main(int argc, char **argv)
{
    size_t len = 0;
    void *mem  = get_mem(argc, argv, len);
		
	RTPSession session;
	
	RTPSessionParams sessionparams;
	sessionparams.SetOwnTimestampUnit(1.0 / 90000.0);
			
	RTPUDPv4TransmissionParams transparams;
	transparams.SetPortbase(8000);
			
	int status = session.Create(sessionparams,&transparams);
	if (status < 0)
	{
		std::cerr << RTPGetErrorString(status) << std::endl;
		exit(-1);
	}
	
	uint8_t localip[] = { 127, 0, 0, 1 };
	RTPIPv4Address addr(localip, 8888);
	
	status = session.AddDestination(addr);
	if (status < 0)
	{
		std::cerr << RTPGetErrorString(status) << std::endl;
		exit(-1);
	}
	
	session.SetDefaultPayloadType(96);
	session.SetDefaultMark(false);
	session.SetDefaultTimestampIncrement(1);

    uint64_t chunk_size, total_size;
    uint64_t fpt_ms = 0;
    uint64_t fsize  = 0;
    uint32_t frames = 0;
    std::chrono::high_resolution_clock::time_point start, fpt_start, fpt_end, end;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < len; ) {
        memcpy(&chunk_size, (uint8_t *)mem + i, sizeof(uint64_t));

        i          += sizeof(uint64_t);
        total_size += chunk_size;

        fpt_start = std::chrono::high_resolution_clock::now();
        push_hevc_chunk(session, (uint8_t *)mem + i, chunk_size);
        fpt_end = std::chrono::high_resolution_clock::now();

        i += chunk_size;
        frames++;
        fsize += chunk_size;
        uint64_t diff = std::chrono::duration_cast<std::chrono::microseconds>(fpt_end - fpt_start).count();
        fpt_ms += diff;
    }
    end = std::chrono::high_resolution_clock::now();

    uint64_t diff = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stderr, "%lu bytes, %lu kB, %lu MB took %u ms %u s\n",
        fsize, fsize / 1000, fsize / 1000 / 1000,
        diff, diff / 1000
    );

    fprintf(stderr, "# of frames: %u\n", frames);
    fprintf(stderr, "avg frame size: %lu\n", fsize / frames);
    fprintf(stderr, "avg processing time of frame: %lu\n", fpt_ms / frames);

    return 0;
}

#if 0
    uint64_t off = 0;
    size_t total = 0;
    std::chrono::high_resolution_clock::time_point start, end;

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        for (size_t k = 0; k < len; ) {
            memcpy(&off, (uint8_t *)mem + k, sizeof(uint64_t));
            k += sizeof(uint64_t);

            status = session.SendPacket(silencebuffer,160);
            if (status < 0)
            {
                std::cerr << RTPGetErrorString(status) << std::endl;
                exit(-1);
            }

            k     += off;
            total += off;
        }
    }

    end = std::chrono::high_resolution_clock::now();

    fprintf(stderr, "took %lu milliseconds to send %zu bytes %zu kB %zu MB\n",
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
        total,
        total / 1000,
        total / 1000 / 1000
    );

		session.BeginDataAccess();
		if (session.GotoFirstSource())
		{
			do
			{
				RTPPacket *packet;

				while ((packet = session.GetNextPacket()) != 0)
				{
					std::cout << "Got packet with " 
					          << "extended sequence number " 
					          << packet->GetExtendedSequenceNumber() 
					          << " from SSRC " << packet->GetSSRC() 
					          << std::endl;
					session.DeletePacket(packet);
				}
			} while (session.GotoNextSource());
		}
		session.EndDataAccess();
#endif
