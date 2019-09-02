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

#define MAX_WRITE_SIZE 1388

int push_hevc_frame(RTPSession& session, uint8_t *mem, size_t chunk_size)
{
    int status;

    if (chunk_size < MAX_WRITE_SIZE) {
        if ((status = session.SendPacket((uint8_t *)mem, chunk_size)) < 0) {
            std::cerr << RTPGetErrorString(status) << std::endl;
            exit(-1);
        }

        return 0;
    }

    for (size_t k = 0; k < chunk_size; k += MAX_WRITE_SIZE) {
        size_t write_size = MAX_WRITE_SIZE;

        if (chunk_size - k < MAX_WRITE_SIZE)
            write_size = chunk_size - k;

        if ((status = session.SendPacket((uint8_t *)mem + k, write_size)) < 0) {
            std::cerr << RTPGetErrorString(status) << std::endl;
            exit(-1);
        }
    }
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
	session.SetDefaultTimestampIncrement(160);

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

        /* Find nal units,  */
        uint8_t start_len = 0;
        int offset        = get_next_frame_start((uint8_t *)mem, 0, chunk_size, start_len);
        int prev_offset   = offset;

        while (offset != -1) {
            offset = get_next_frame_start((uint8_t *)mem, offset, chunk_size, start_len);

            if (offset > 4) {
                push_hevc_frame(session, (uint8_t *)mem + prev_offset, chunk_size - prev_offset);
                prev_offset = offset;
            }
        }

        if (prev_offset == -1)
            prev_offset = 0;

        push_hevc_frame(session, (uint8_t *)mem + prev_offset, chunk_size - prev_offset);

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
