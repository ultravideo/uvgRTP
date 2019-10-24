#pragma once

#include <condition_variable>
#include <queue>
#include <thread>

#include "queue.hh"
#include "socket.hh"
#include "runner.hh"
#include "util.hh"

namespace kvz_rtp {

    /* System call dispatcher is an optimization technique which aims to minimize
     * the delay application experiences when calling push_frame().
     *
     * The push_frame() is divided roughly into frontend and backend:
     * Frontend is the part that executes in the context of the calling application. During this
     * phase, the HEVC frame is packetized into smaller frames and put into frame queue.
     * On Linux this step is very fast, the application only loops through the HEVC frame and
     * sets pointers to frame queue.
     * On windows it's a little more work because we can't send multiple packets using one system call
     * AND use scatter-gather I/O so it's either or.
     *
     * When the frame has been split into smaller chunks, the frontend will call the backend
     * using trigger_send() functions. This function signals the dispatcher thread that there is
     * a frame waiting to be sent. When trigger_send() returns, the application exists from the
     * library code and the frame is sent in the background.
     *
     * By using a separate dispatcher thread, we're able to reduce the amount of delay application
     * experiences to very small (<50 us even for large frames [>170 kB]) */
    typedef struct transaction transaction_t;

    class dispatcher : public runner {
        public:
            dispatcher(kvz_rtp::socket *socket);
            ~dispatcher();

            /* Add new transaction to dispatcher's task queue 
             * The task queue is emptied in FIFO style */
            rtp_error_t trigger_send(kvz_rtp::transaction_t *transaction);

            /* Application and dispatcher communicate with each other using a condition variable 
             *
             * If the task queue is empty, dispatcher will wait on a condition variable
             * and when application pushes a new transaction to task queue, it will notify
             * the dispatcher that it can start the send process. */
            std::condition_variable& get_cvar();

            /* Get next transaction from task queue  
             * Return nullptr if the task queue is empty */
            kvz_rtp::transaction_t *get_transaction();

        private:
            static void dispatch_runner(kvz_rtp::dispatcher *dispatcher, kvz_rtp::socket *socket);

            std::condition_variable cv_;

            std::mutex q_mtx_;
            std::queue<kvz_rtp::transaction_t *> tasks_;

            kvz_rtp::socket *socket_;
    };
};
