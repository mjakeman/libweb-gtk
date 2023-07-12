/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Utilities.h"
#include <AK/DeprecatedString.h>
#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/button.h>
#include <glibmm/main.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>

//#include "WebContentView.h"
#include "Embed/webembed.h"
#include "Embed/webcontentview.h"

GtkApplication *global_app;

static void on_activate (GtkApplication *app) {

    global_app = app;
    g_application_hold(reinterpret_cast<GApplication *>(app));


    auto source = Glib::TimeoutSource::create(0);
    source->connect([]() -> bool {

        GtkWidget *window = gtk_application_window_new(global_app);
        gtk_window_set_default_size(GTK_WINDOW (window), 720, 480);
        gtk_window_set_title(GTK_WINDOW (window), "LibWebGTK");

        GtkWidget *view = web_content_view_new ();
        GtkWidget *view2 = web_content_view_new ();

//        auto view = new ContentViewImpl(String(), WebView::EnableCallgrindProfiling::No, WebView::UseJavaScriptBytecode::Yes);
//        auto view2 = new ContentViewImpl(String(), WebView::EnableCallgrindProfiling::No, WebView::UseJavaScriptBytecode::Yes);

        Gtk::Entry navigation = Gtk::Entry();
        navigation.set_hexpand(true);

        Gtk::Button button = Gtk::Button("Go!");
        button.signal_clicked().connect([&]() {
            auto url = navigation.get_buffer().get()->get_text();
            web_content_view_load(WEB_CONTENT_VIEW(view), url.c_str());
        });

        Gtk::Box controls = Gtk::Box(Gtk::Orientation::HORIZONTAL);
        controls.add_css_class("toolbar");
        controls.append(navigation);
        controls.append(button);

        Gtk::Paned paned = Gtk::Paned();
        Gtk::ScrolledWindow scroll_area = Gtk::ScrolledWindow();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_area.gobj()), view);
        scroll_area.set_vexpand(true);
        paned.set_start_child(scroll_area);

        Gtk::ScrolledWindow scroll_area_2 = Gtk::ScrolledWindow();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_area_2.gobj()), view2);
        scroll_area_2.set_vexpand(true);
        paned.set_end_child(scroll_area_2);

        Gtk::Box box = Gtk::Box(Gtk::Orientation::VERTICAL);
        box.append(controls);
        box.append(paned);

        gtk_window_set_child(GTK_WINDOW (window), GTK_WIDGET (box.gobj()));
        gtk_window_present(GTK_WINDOW(window));

        web_content_view_load(WEB_CONTENT_VIEW(view), "https://mattjakeman.com/");
        web_content_view_load(WEB_CONTENT_VIEW(view2), "https://awesomekling.github.io/Ladybird-a-new-cross-platform-browser-project/");

        g_application_release(reinterpret_cast<GApplication *>(global_app));

        return false;
    });
    source->attach(Glib::MainContext::get_default());
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
