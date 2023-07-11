/*
 * Copyright (c) 2022-2023, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "EventLoopImplementationGtk.h"
#include <AK/IDAllocator.h>
#include <LibCore/Event.h>
#include <LibCore/Notifier.h>
#include <LibCore/Object.h>
#include <LibCore/ThreadEventQueue.h>
#include <glibmm/main.h>

namespace Ladybird {

struct ThreadData;
static thread_local ThreadData* s_thread_data;

struct ThreadData {
    static ThreadData& the()
    {
        if (!s_thread_data) {
            // FIXME: Don't leak this.
            s_thread_data = new ThreadData;
        }
        return *s_thread_data;
    }

    IDAllocator timer_id_allocator;
    HashMap<int, Glib::RefPtr<Glib::TimeoutSource>> timers;
    HashMap<Core::Notifier*, Glib::RefPtr<Glib::IOSource>> notifiers;
    HashMap<int, Glib::RefPtr<Glib::IOChannel>> fd_channels;
};

EventLoopImplementationGtk::EventLoopImplementationGtk()
{
    dbgln("INTEGRATION {} Create", getpid());
//    m_event_loop = g_main_loop_new(nullptr, FALSE);
}

EventLoopImplementationGtk::~EventLoopImplementationGtk()
{
    dbgln("INTEGRATION {} Delete", getpid());
//    g_main_loop_unref(m_event_loop);
}

int EventLoopImplementationGtk::exec()
{
    dbgln("INTEGRATION {} Exec", getpid());
//    g_main_loop_run(m_event_loop);
    GMainContext *context = g_main_context_default();
    while (true)
        g_main_context_iteration(context, TRUE);
    dbgln("Context Grabbed: {}", context);
    return m_error_code;
}

size_t EventLoopImplementationGtk::pump(PumpMode mode)
{
    VERIFY_NOT_REACHED();
    dbgln("INTEGRATION {} Pump", getpid());
    auto result = Core::ThreadEventQueue::current().process();
    if (mode == PumpMode::WaitForEvents) {
//        g_main_context_iteration(g_main_loop_get_context(m_event_loop), TRUE);
    } else {
    }
    result += Core::ThreadEventQueue::current().process();
    return result;
}

void EventLoopImplementationGtk::quit(int code)
{
    VERIFY_NOT_REACHED();
    dbgln("INTEGRATION {} Quit", getpid());
    m_error_code = code;
//    g_main_loop_quit(m_event_loop);
}

void EventLoopImplementationGtk::wake() {}

void EventLoopImplementationGtk::post_event(Core::Object& receiver, NonnullOwnPtr<Core::Event>&& event)
{
    dbgln("INTEGRATION {} Post event", getpid());
    // Can we have multithreaded event queues?
    m_thread_event_queue.post_event(receiver, move(event));
    if (&m_thread_event_queue != &Core::ThreadEventQueue::current())
        wake();
}

static void glib_timer_fired(int timer_id, Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible, Core::Object& object)
{
    dbgln("INTEGRATION {} Timer fired", getpid());
    if (should_fire_when_not_visible == Core::TimerShouldFireWhenNotVisible::No) {
        if (!object.is_visible_for_timer_purposes())
            return;
    }
    Core::TimerEvent event(timer_id);
    object.dispatch_event(event);
}

int EventLoopManagerGtk::register_timer(Core::Object& object, int milliseconds, bool should_reload, Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible)
{
    dbgln("INTEGRATION {} Register timer", getpid());
    auto& thread_data = ThreadData::the();

    auto timer_id = thread_data.timer_id_allocator.allocate();
    auto weak_object = object.make_weak_ptr();

    auto source = Glib::TimeoutSource::create(milliseconds);
    source->connect([timer_id, should_fire_when_not_visible, should_reload, weak_object = move(weak_object)]() -> bool {
        auto object = weak_object.strong_ref();
        if (!object)
            return false;
        glib_timer_fired(timer_id, should_fire_when_not_visible, *object);
        return should_reload;
    });
    source->attach(Glib::MainContext::get_default());
    thread_data.timers.set(timer_id, move(source));

    // dbgln("Registered timer (id={}, object={}, ms={}, reload={}, fire_not_vis={})", timer_id, &object, milliseconds, should_reload, (int)should_fire_when_not_visible);

    return timer_id;
}

bool EventLoopManagerGtk::unregister_timer(int timer_id)
{
    dbgln("INTEGRATION {} Unregister timer", getpid());
    auto& thread_data = ThreadData::the();
    thread_data.timer_id_allocator.deallocate(timer_id);

    auto timer = thread_data.timers.get(timer_id);
    if (timer.has_value()) {
        timer->get()->destroy();
    }

    // dbgln("Unregistered timer (timer_id={})", timer_id);

    return thread_data.timers.remove(timer_id);
}

void EventLoopManagerGtk::register_notifier(Core::Notifier& notifier)
{
    dbgln("INTEGRATION {} Register notifier", getpid());
    Glib::IOCondition condition;
    switch (notifier.type()) {
    case Core::Notifier::Type::Read:
        condition = Glib::IOCondition::IO_IN;
        break;
    case Core::Notifier::Type::Write:
        condition = Glib::IOCondition::IO_OUT;
        break;
    default:
        TODO();
    }

    auto fd = notifier.fd();
    auto thread_data = ThreadData::the();

    Glib::RefPtr<Glib::IOChannel> channel;

    // Get existing channel
    auto maybe_channel = thread_data.fd_channels.get(fd);
    if (maybe_channel.has_value()) {
        channel = maybe_channel.value();
        channel->reference();
    } else {
        channel = Glib::IOChannel::create_from_fd(notifier.fd());
        thread_data.fd_channels.set(fd, channel);

        // FIXME: Add callback to remove from hashmap
        // channel->add_destroy_notify_callback(nullptr, ...);
    }

    auto io_watch = channel->create_watch(condition);

    io_watch->connect([condition, &notifier](Glib::IOCondition cond) -> bool {
        if (cond == condition) {
            Core::NotifierActivationEvent event(notifier.fd());
            notifier.dispatch_event(event);
        }
        return G_SOURCE_CONTINUE;
    });
    io_watch->attach(Glib::MainContext::get_default());

    // Store watch ID for later
    // dbgln("Added notifier (key={}, value={})", &notifier, (int)watch_id);
    ThreadData::the().notifiers.set(&notifier, move(io_watch));
}

void EventLoopManagerGtk::unregister_notifier(Core::Notifier& notifier)
{
    dbgln("INTEGRATION {} Unregister notifier", getpid());
    auto thread_data = ThreadData::the();

    auto watch = thread_data.notifiers.get(&notifier);
    if (watch.has_value()) {
        watch->get()->destroy();
    }

    auto channel = thread_data.fd_channels.get(notifier.fd());
    if (channel.has_value()) {
        channel->get()->unreference();
    }

    ThreadData::the().notifiers.remove(&notifier);
}

void cb_process_events2() {
    Core::ThreadEventQueue::current().process();
}

void EventLoopManagerGtk::did_post_event()
{
    dbgln("INTEGRATION {} Posted event", getpid());
    g_timeout_add_once(0, reinterpret_cast<GSourceOnceFunc>(cb_process_events2), nullptr);
}

EventLoopManagerGtk::EventLoopManagerGtk()
{
    g_timeout_add_once(0, reinterpret_cast<GSourceOnceFunc>(cb_process_events2), nullptr);
}

EventLoopManagerGtk::~EventLoopManagerGtk() = default;

NonnullOwnPtr<Core::EventLoopImplementation> EventLoopManagerGtk::make_implementation()
{
    return adopt_own(*new EventLoopImplementationGtk);
}

}
