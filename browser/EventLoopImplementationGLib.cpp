/*
 * Copyright (c) 2022-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "EventLoopImplementationGLib.h"
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
    HashMap<Core::Notifier*, int> notifiers;
};

EventLoopImplementationGLib::EventLoopImplementationGLib()
{
    m_event_loop = g_main_loop_new(NULL, FALSE);
}

EventLoopImplementationGLib::~EventLoopImplementationGLib()
{
    g_main_loop_unref(m_event_loop);
}

int EventLoopImplementationGLib::exec()
{
    g_main_loop_run(m_event_loop);
    return m_error_code;
}

size_t EventLoopImplementationGLib::pump(PumpMode mode)
{
    auto result = Core::ThreadEventQueue::current().process();
    if (mode == PumpMode::WaitForEvents) {
        g_main_context_iteration(g_main_loop_get_context(m_event_loop), TRUE);
    } else {
    }
    result += Core::ThreadEventQueue::current().process();
    return result;
}

void EventLoopImplementationGLib::quit(int code)
{
    m_error_code = code;
    g_main_loop_quit(m_event_loop);
}

void EventLoopImplementationGLib::wake() {}

void EventLoopImplementationGLib::post_event(Core::Object& receiver, NonnullOwnPtr<Core::Event>&& event)
{
    // Can we have multithreaded event queues?
    m_thread_event_queue.post_event(receiver, move(event));
    if (&m_thread_event_queue != &Core::ThreadEventQueue::current())
        wake();
}

static void glib_timer_fired(int timer_id, Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible, Core::Object& object)
{
    if (should_fire_when_not_visible == Core::TimerShouldFireWhenNotVisible::No) {
        if (!object.is_visible_for_timer_purposes())
            return;
    }
    Core::TimerEvent event(timer_id);
    object.dispatch_event(event);
}

int EventLoopManagerGLib::register_timer(Core::Object& object, int milliseconds, bool should_reload, Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible)
{
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

    return timer_id;
}

bool EventLoopManagerGLib::unregister_timer(int timer_id)
{
    auto& thread_data = ThreadData::the();
    thread_data.timer_id_allocator.deallocate(timer_id);

    auto timer = thread_data.timers.get(timer_id);
    if (timer.has_value()) {
        timer->get()->destroy();
    }
    return thread_data.timers.remove(timer_id);
}

static void glib_notifier_activated(GIOChannel *source, GIOCondition, Core::Notifier& notifier)
{
    Core::NotifierActivationEvent event(g_io_channel_unix_get_fd (source));
    notifier.dispatch_event(event);
}

void EventLoopManagerGLib::register_notifier(Core::Notifier& notifier)
{
    GIOCondition condition;
    switch (notifier.type()) {
    case Core::Notifier::Type::Read:
        condition = G_IO_IN;
        break;
    case Core::Notifier::Type::Write:
        condition = G_IO_OUT;
        break;
    default:
        TODO();
    }

    GIOChannel* channel = g_io_channel_unix_new(notifier.fd());
    guint watch_id = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, condition, (GIOFunc)glib_notifier_activated, &notifier,
                                         reinterpret_cast<GDestroyNotify>(g_io_channel_unref));

    // Store watch ID for later
    dbgln("Added notifier (key={}, value={})", &notifier, (int)watch_id);
    ThreadData::the().notifiers.set(&notifier, watch_id);
}

void EventLoopManagerGLib::unregister_notifier(Core::Notifier& notifier)
{
    auto watch_id = ThreadData::the().notifiers.get(&notifier);
    if (watch_id.has_value())
        g_source_remove(watch_id.value());
    ThreadData::the().notifiers.remove(&notifier);
    dbgln("Removed notifier (key={}, value={})", &notifier, watch_id);
}

void cb_process_events() {
    Core::ThreadEventQueue::current().process();
}

void EventLoopManagerGLib::did_post_event()
{
    g_timeout_add_once(0, reinterpret_cast<GSourceOnceFunc>(cb_process_events), nullptr);
}

EventLoopManagerGLib::EventLoopManagerGLib()
{
    g_timeout_add_once(0, reinterpret_cast<GSourceOnceFunc>(cb_process_events), nullptr);
}

EventLoopManagerGLib::~EventLoopManagerGLib() = default;

NonnullOwnPtr<Core::EventLoopImplementation> EventLoopManagerGLib::make_implementation()
{
    return adopt_own(*new EventLoopImplementationGLib);
}

}
