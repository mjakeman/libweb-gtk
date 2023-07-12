/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Embed/webembed.h"
#include "Embed/webcontentview.h"

static void on_activate (GtkApplication *app) {

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW (window), 720, 480);
    gtk_window_set_title(GTK_WINDOW (window), "LibWebGTK");

    GtkWidget *view_left = web_content_view_new ();
    GtkWidget *view_right = web_content_view_new ();

    GtkWidget *navigation = gtk_entry_new();
    gtk_widget_set_hexpand(navigation, true);

    GtkWidget *button = gtk_button_new_with_label ("Go!");
//    button.signal_clicked().connect([&]() {
//        auto url = navigation.get_buffer().get()->get_text();
//        web_content_view_load(WEB_CONTENT_VIEW(view), url.c_str());
//    });

    GtkWidget *controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class (controls, "toolbar");
    gtk_box_append(GTK_BOX (controls), navigation);
    gtk_box_append(GTK_BOX (controls), button);

    GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);

    GtkWidget *scroll_area_left = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll_area_left), view_left);
    gtk_widget_set_vexpand (GTK_WIDGET(scroll_area_left), TRUE);
    gtk_paned_set_start_child (GTK_PANED (paned), scroll_area_left);

    GtkWidget *scroll_area_right = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll_area_right), view_right);
    gtk_widget_set_vexpand (GTK_WIDGET(scroll_area_right), TRUE);
    gtk_paned_set_end_child (GTK_PANED (paned), scroll_area_right);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX (box), controls);
    gtk_box_append(GTK_BOX (box), paned);

    gtk_window_set_child(GTK_WINDOW (window), GTK_WIDGET (box));
    gtk_window_present(GTK_WINDOW(window));

    web_content_view_load(WEB_CONTENT_VIEW(view_left), "https://mattjakeman.com/");
    web_content_view_load(WEB_CONTENT_VIEW(view_right), "https://awesomekling.github.io/Ladybird-a-new-cross-platform-browser-project/");
}

int main(int argc, char **argv)
{
    GtkApplication *app;

    app = gtk_application_new("com.mattjakeman.LibWebGTK", G_APPLICATION_DEFAULT_FLAGS);

    gtk_init();
    web_embed_init();

    g_signal_connect(app, "activate", G_CALLBACK (on_activate), NULL);

    return g_application_run(G_APPLICATION(app), argc, argv);
}
