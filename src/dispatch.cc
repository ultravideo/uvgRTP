#include "debug.hh"
#include "dispatch.hh"
#include "socket.hh"

#ifndef _WIN32
kvz_rtp::dispatcher::dispatcher(kvz_rtp::socket *socket):
    socket_(socket)
{
    LOG_ERROR("starting system call dispatcher!");
}

kvz_rtp::dispatcher::~dispatcher()
{
}

rtp_error_t kvz_rtp::dispatcher::start()
{
    if ((runner_ = new std::thread(dispatch_runner, this, socket_)) == nullptr)
        return RTP_MEMORY_ERROR;

    runner_->detach();

    return kvz_rtp::runner::start();
}

rtp_error_t kvz_rtp::dispatcher::stop()
{
    if (tasks_.size() > 0)
        return RTP_NOT_READY;

    /* notify dispatcher that we must stop now, lock the dispatcher mutex
     * and wait until it's unlocked by the dispatcher at which point it is safe
     * to return and release all memory */
    (void)kvz_rtp::runner::stop();
    d_mtx_.lock();
    cv_.notify_one();
    while (d_mtx_.try_lock());

    return RTP_OK;
}

std::condition_variable& kvz_rtp::dispatcher::get_cvar()
{
    return cv_;
}

std::mutex& kvz_rtp::dispatcher::get_mutex()
{
    return d_mtx_;
}

rtp_error_t kvz_rtp::dispatcher::trigger_send(kvz_rtp::transaction_t *t)
{
    std::lock_guard<std::mutex> lock(d_mtx_);

    if (!t)
        return RTP_INVALID_VALUE;

    tasks_.push(t);
    cv_.notify_one();

    return RTP_OK;
}

kvz_rtp::transaction_t *kvz_rtp::dispatcher::get_transaction()
{
    std::lock_guard<std::mutex> lock(d_mtx_);

    if (tasks_.size() == 0)
        return nullptr;

    auto elem = tasks_.front();
    tasks_.pop();

    return elem;
}

void kvz_rtp::dispatcher::dispatch_runner(kvz_rtp::dispatcher *dispatcher, kvz_rtp::socket *socket)
{
    if (!dispatcher || !socket) {
        LOG_ERROR("System call dispatcher cannot continue, invalid value given!");
        return;
    }

    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    kvz_rtp::transaction_t *t = nullptr;

    while (!dispatcher->active())
        ;

    while (dispatcher->active()) {
        if ((t = dispatcher->get_transaction()) == nullptr) {
            dispatcher->get_cvar().wait(lk);
            t = dispatcher->get_transaction();

            if (t == nullptr)
                break;
        }

        do {
            socket->send_vecio(t->headers, t->hdr_ptr, 0);

            if (t->fqueue)
                t->fqueue->deinit_transaction(t->key);
        } while ((t = dispatcher->get_transaction()) != nullptr);

        dispatcher->get_cvar().notify_one();
    }

    dispatcher->get_mutex().unlock();
}
#endif
