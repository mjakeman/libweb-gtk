/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Utilities.h"
#include <AK/DeprecatedString.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/button.h>
#include <gtkmm/applicationwindow.h>

#include "WebContentView.h"
#include "Embed/webembed.h"

static void on_activate (GtkApplication *app) {
    GtkWidget *window = gtk_application_window_new(app);

    WebContentView view(String(), WebView::EnableCallgrindProfiling::No, WebView::UseJavaScriptBytecode::Yes);

    Gtk::Entry navigation = Gtk::Entry();
    navigation.set_hexpand(true);

    Gtk::Button button = Gtk::Button("Go!");
    button.signal_clicked().connect([&]() {
        auto url = navigation.get_buffer().get()->get_text();
        view.load(ak_deprecated_string_from_ustring(url).value());
    });

    view.on_load_start = [&](URL url, bool) {
        navigation.get_buffer()->set_text(ustring_from_ak_string(url.to_string().value()));
    };

    Gtk::Box controls = Gtk::Box(Gtk::Orientation::HORIZONTAL);
    controls.add_css_class("toolbar");
    controls.append(navigation);
    controls.append(button);

    Gtk::ScrolledWindow scroll_area = Gtk::ScrolledWindow();
    scroll_area.set_child(view);
    scroll_area.set_vexpand(true);

    Gtk::Box box = Gtk::Box(Gtk::Orientation::VERTICAL);
    box.append(controls);
    box.append(scroll_area);

    gtk_window_set_child(GTK_WINDOW (window), GTK_WIDGET (box.gobj()));
    gtk_window_present(GTK_WINDOW(window));

    view.load(AK::DeprecatedString("https://mattjakeman.com/"));

    // this works, but... why?
    GMainContext *context = g_main_context_default();
    while (true)
        g_main_context_iteration(context, TRUE);

    VERIFY_NOT_REACHED();
}

int main(int argc, char **argv)
{
    Gtk::Application::create("org.gtkmm.examples.base");
    GtkApplication *app = gtk_application_new("com.mattjakeman.LibWebGTK", G_APPLICATION_DEFAULT_FLAGS);

    gtk_init();
    webembed_init();

    g_signal_connect(app, "activate", G_CALLBACK (on_activate), NULL);

    return g_application_run(G_APPLICATION(app), argc, argv);
}
