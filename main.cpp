/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/ArgsParser.h>
#include <LibCore/Timer.h>
#include <LibMain/Main.h>

#include <gtk/gtk.h>

/*extern void initialize_web_engine();
Browser::Settings* s_settings;*/

static void
app_activate (GApplication *app, gpointer *user_data) {
    (void) app;
    (void) user_data;

    g_print ("GtkApplication is activated.\n");

    GtkWidget *window;
    window = gtk_application_window_new (GTK_APPLICATION (app));
    gtk_window_set_title (GTK_WINDOW (window), "Ladybird GTK");
    gtk_window_present (GTK_WINDOW (window));
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    (void) arguments;

    GtkApplication *app;
    int stat;

    app = gtk_application_new ("com.github.ToshioCP.pr1", G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK (app_activate), NULL);
    stat = g_application_run (G_APPLICATION (app), 0, NULL);
    g_object_unref (app);
    return stat;
}
