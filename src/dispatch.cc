#include "debug.hh"
#include "dispatch.hh"
#include "socket.hh"

#ifdef __RTP_USE_SYSCALL_DISPATCHER__
kvz_rtp::dispatcher::dispatcher(kvz_rtp::socket *socket):
    socket_(socket)
{
}

kvz_rtp::dispatcher::~dispatcher()
{
}

rtp_error_t kvz_rtp::dispatcher::start()
{
    if ((runner_ = new std::thread(dispatch_runner, this, socket_)) == nullptr)
        return RTP_MEMORY_ERROR;

    return kvz_rtp::runner::start();
}

rtp_error_t kvz_rtp::dispatcher::stop()
{
    if (tasks_.size() > 0)
        return RTP_NOT_READY;

    return kvz_rtp::runner::stop();
}

std::condition_variable& kvz_rtp::dispatcher::get_cvar()
{
    return cv_;
}

rtp_error_t kvz_rtp::dispatcher::trigger_send(kvz_rtp::transaction_t *t)
{
    std::lock_guard<std::mutex> lock(q_mtx_);

    if (!t)
        return RTP_INVALID_VALUE;

    tasks_.push(t);
    cv_.notify_one();

    return RTP_OK;
}

kvz_rtp::transaction_t *kvz_rtp::dispatcher::get_transaction()
{
    std::lock_guard<std::mutex> lock(q_mtx_);

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

    while (dispatcher->active()) {
        if ((t = dispatcher->get_transaction()) == nullptr) {
            dispatcher->get_cvar().wait(lk);
            t = dispatcher->get_transaction();
        }

        do {
            socket->send_vecio(t->headers, t->hdr_ptr, 0);

            if (t->fqueue)
                t->fqueue->deinit_transaction(t->key);
        } while ((t = dispatcher->get_transaction()) != nullptr);
    }
}
#endif
