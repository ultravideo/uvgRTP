#include "dispatch.hh"

#include "queue.hh"
#include "socket.hh"
#include "debug.hh"


#ifndef _WIN32
uvgrtp::dispatcher::dispatcher(uvgrtp::socket *socket):
    socket_(socket)
{
}

uvgrtp::dispatcher::~dispatcher()
{
}

rtp_error_t uvgrtp::dispatcher::start()
{
    runner_ = new std::thread(dispatch_runner, this, socket_);
    runner_->detach();

    return uvgrtp::runner::start();
}

rtp_error_t uvgrtp::dispatcher::stop()
{
    if (tasks_.size() > 0)
        return RTP_NOT_READY;

    /* notify dispatcher that we must stop now, lock the dispatcher mutex
     * and wait until it's unlocked by the dispatcher at which point it is safe
     * to return and release all memory */
    (void)uvgrtp::runner::stop();
    d_mtx_.lock();
    cv_.notify_one();
    while (d_mtx_.try_lock());

    return RTP_OK;
}

std::condition_variable& uvgrtp::dispatcher::get_cvar()
{
    return cv_;
}

std::mutex& uvgrtp::dispatcher::get_mutex()
{
    return d_mtx_;
}

rtp_error_t uvgrtp::dispatcher::trigger_send(uvgrtp::transaction_t *t)
{
    std::lock_guard<std::mutex> lock(d_mtx_);

    if (!t)
        return RTP_INVALID_VALUE;

    tasks_.push(t);
    cv_.notify_one();

    return RTP_OK;
}

uvgrtp::transaction_t *uvgrtp::dispatcher::get_transaction()
{
    std::lock_guard<std::mutex> lock(d_mtx_);

    if (tasks_.size() == 0)
        return nullptr;

    auto elem = tasks_.front();
    tasks_.pop();

    return elem;
}

void uvgrtp::dispatcher::dispatch_runner(uvgrtp::dispatcher *dispatcher, uvgrtp::socket *socket)
{
    if (!dispatcher || !socket) {
        LOG_ERROR("System call dispatcher cannot continue, invalid value given!");
        return;
    }

    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    uvgrtp::transaction_t *t = nullptr;

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
            /* socket->send_vecio(t->headers, t->hdr_ptr, 0); */

            if (t->fqueue)
                t->fqueue->deinit_transaction(t->key);
        } while ((t = dispatcher->get_transaction()) != nullptr);

        dispatcher->get_cvar().notify_one();
    }

    dispatcher->get_mutex().unlock();
}
#endif
