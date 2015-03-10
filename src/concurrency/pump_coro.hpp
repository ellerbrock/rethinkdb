// Copyright 2010-2015 RethinkDB, all rights reserved.
#ifndef CONCURRENCY_PUMP_CORO_HPP_
#define CONCURRENCY_PUMP_CORO_HPP_

#include <functional>
#include <map>
#include <vector>

#include "concurrency/auto_drainer.hpp"
#include "concurrency/mutex_assertion.hpp"
#include "containers/counted.hpp"

/* `pump_coro_t` is a class for managing a blocking function that must eventually run
after a certain event occurs; but if the event occurs multiple times in quick succession,
it's OK for the function to only run once. For example, we might want to flush a value to
disk every time it changes. */

class pump_coro_t : public home_thread_mixin_debug_only_t{
public:
    /* The constructor specifies which callback should be called. The callback may block.
    If the `pump_coro_t` is destroyed its interruptor will be pulsed and it may throw
    `interrupted_exc_t`. The callback is guaranteed to stop before the `pump_coro_t`
    destructor returns. `max_callbacks` is the maximum number of instances of the
    callback that can be running at once. */
    pump_coro_t(
        const std::function<void(signal_t *)> &callback,
        size_t max_callbacks = 1);

    /* `notify()` schedules the callback to be run, unless it's already scheduled. */
    void notify();

    /* Normally, if `notify()` is called while the callback is running, then the callback
    will be called again after it returns. But if the callback calls
    `include_latest_notifications()`, all notifications up to that point will be
    considered to be handled by this instance of the callback. This is only legal if
    `max_callbacks` is 1. */
    void include_latest_notifications();

    /* `flush()` blocks until all the calls to `notify()` up to this point have been
    handled. */
    void flush(signal_t *interruptor);

private:
    class counted_cond_t :
        public cond_t,
        public single_threaded_countable_t<counted_cond_t>
        { };

    void run(auto_drainer_t::lock_t keepalive);

    std::function<void(signal_t *)> const callback;
    size_t const max_callbacks;

    /* `mutex` is used to protect any access to the variables below, to prevent
    unexpected blocking. */
    mutex_assertion_t mutex;

    /* `running` is the number of callbacks that are currently running, not counting
    coroutines that are starting. `starting` is `true` if there is a coroutine starting.
    `queued` is `true` if we hit the `max_callbacks` limit and need to run the callback
    again as soon as one of the running instances finishes. */
    size_t running;
    bool starting, queued;

    /* `flush()` inserts into `flush_waiters`; `run()` makes a local copy of
    `flush_waiters` and then clears it. When the callback finishes, it pulses all of the
    conds in its local copy. */
    std::vector<counted_t<counted_cond_t> > flush_waiters;

    /* If `max_callbacks` is 1, then `run()` will set `running_flush_waiters` to its
    local copy of `flush_waiters`, so that `include_latest_notifications()` can access
    it. */
    std::vector<counted_t<counted_cond_t> > *running_flush_waiters;

    auto_drainer_t drainer;
};

#endif /* CONCURRENCY_PUMP_CORO_HPP_ */

