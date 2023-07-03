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

namespace Ladybird {

struct ThreadData;
static thread_local ThreadData* s_thread_data;

struct ThreadData {
    static ThreadData& the()
    {
        if (!s_thread_data) {
            // FIXME: Don't leak this.
            s_thread_data = new ThreadData;
            s_thread_data->notifiers = g_hash_table_new(nullptr, nullptr);
        }
        return *s_thread_data;
    }

    IDAllocator timer_id_allocator;
    GHashTable* notifiers;
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

typedef struct {
    bool should_reload;
    Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible;
    WeakPtr<Core::Object> weak_object;
} TimerData;

static int glib_timer_fired(int timer_id, TimerData *data)
{
    if (!data)
        return G_SOURCE_REMOVE;

    auto object = data->weak_object.strong_ref();
    if (!object)
        return G_SOURCE_REMOVE;

    if (data->should_fire_when_not_visible == Core::TimerShouldFireWhenNotVisible::No) {
        if (!object->is_visible_for_timer_purposes()) {
            if (data->should_reload) {
                return G_SOURCE_CONTINUE;
            } else {
                return G_SOURCE_REMOVE;
            }
        }
    }
    Core::TimerEvent event(timer_id);
    object->dispatch_event(event);

    if (data->should_reload) {
        return G_SOURCE_CONTINUE;
    } else {
        return G_SOURCE_REMOVE;
    }
}

static void glib_timer_destroy(TimerData *data) {
    g_free(data);
}

int EventLoopManagerGLib::register_timer(Core::Object& object, int milliseconds, bool should_reload, Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible)
{
    auto *data = g_new0(TimerData,1);
    data->should_reload = should_reload;
    data->weak_object = object.make_weak_ptr();
    data->should_fire_when_not_visible = should_fire_when_not_visible;
    guint timer_id = g_timeout_add_full(G_PRIORITY_DEFAULT, milliseconds, reinterpret_cast<GSourceFunc>(glib_timer_fired), data,
                                        reinterpret_cast<GDestroyNotify>(glib_timer_destroy));
    return (int) timer_id;
}

bool EventLoopManagerGLib::unregister_timer(int timer_id)
{
    return g_source_remove(timer_id);
}

typedef struct {
    bool should_reload;
    Core::TimerShouldFireWhenNotVisible should_fire_when_not_visible;
    WeakPtr<Core::Object>& weak_object;
} NotifierData;

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

    GHashTable *table = ThreadData::the().notifiers;
    GIOChannel* channel = g_io_channel_unix_new(notifier.fd());
    guint watch_id = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, condition, (GIOFunc)glib_notifier_activated, &notifier,
                                         reinterpret_cast<GDestroyNotify>(g_io_channel_unref));

    // Store watch ID for later
    g_hash_table_insert(table, &notifier, GINT_TO_POINTER(watch_id));
}

void EventLoopManagerGLib::unregister_notifier(Core::Notifier& notifier)
{
    GHashTable *table = ThreadData::the().notifiers;
    guint watch_id = GPOINTER_TO_INT(g_hash_table_lookup(table, &notifier));
    g_source_remove(watch_id);
    g_hash_table_remove(table, &notifier);
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
