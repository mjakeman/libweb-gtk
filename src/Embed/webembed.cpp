/*
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "webembed.h"
#include "LibCore/EventLoopImplementation.h"
#include "EventLoopImplementationGLib.h"
#include "LibCore/EventLoop.h"
#include "Utilities.h"
#include "LibGfx/Font/FontDatabase.h"

#include <gtk/gtk.h>

static ErrorOr<void> handle_attached_debugger()
{
#ifdef AK_OS_LINUX
    // Let's ignore SIGINT if we're being debugged because GDB
    // incorrectly forwards the signal to us even when it's set to
    // "nopass". See https://sourceware.org/bugzilla/show_bug.cgi?id=9425
    // for details.
    auto unbuffered_status_file = TRY(Core::File::open("/proc/self/status"sv, Core::File::OpenMode::Read));
    auto status_file = TRY(Core::InputBufferedFile::create(move(unbuffered_status_file)));
    auto buffer = TRY(ByteBuffer::create_uninitialized(4096));
    while (TRY(status_file->can_read_line())) {
        auto line = TRY(status_file->read_line(buffer));
        auto const parts = line.split_view(':');
        if (parts.size() < 2 || parts[0] != "TracerPid"sv)
            continue;
        auto tracer_pid = parts[1].to_uint<u32>();
        if (tracer_pid != 0UL) {
            dbgln("Debugger is attached, ignoring SIGINT");
            TRY(Core::System::signal(SIGINT, SIG_IGN));
        }
        break;
    }
#endif
    return {};
}

#include "AK/Error.h"
#include <LibCore/EventLoop.h>
#include <memory>
#include <gtkmm/init.h>

std::unique_ptr<Core::EventLoop> event_loop_ptr;

void webembed_init()
{
    gtk_init();
    Gtk::init_gtkmm_internals();

    // Setup utility methods for GLib event loop integration
    //  -> Note that EventLoopManagerGLib operates on the default event loop, so it can reuse
    //     that of GtkApplication and work transparently
    //  -> Theoretically, anyway...
    Core::EventLoopManager::install(*new Ladybird::EventLoopManagerGLib);
    event_loop_ptr = std::make_unique<Core::EventLoop>(); // Create main loop and keep it around

    auto _ = handle_attached_debugger();

    platform_init();

    // NOTE: We only instantiate this to ensure that Gfx::FontDatabase has its default queries initialized.
    Gfx::FontDatabase::set_default_font_query("Katica 10 400 0");
    Gfx::FontDatabase::set_fixed_width_font_query("Csilla 10 400 0");
}