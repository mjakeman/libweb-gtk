/*
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WEB_TYPE_CONTENT_VIEW (web_content_view_get_type())

G_DECLARE_FINAL_TYPE (WebContentView, web_content_view, WEB, CONTENT_VIEW, GtkWidget)

GtkWidget *
web_content_view_new ();

void
web_content_view_load (WebContentView *self, const char *url);

G_END_DECLS
