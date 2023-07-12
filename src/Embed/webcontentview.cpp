/*
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "webcontentview.h"
#include "ContentViewImpl.h"
#include "Utilities.h"

#include <memory>
#include <optional>

struct _WebContentView
{
    GtkWidget parent_instance;
    std::optional<ContentViewImpl> view_impl;

    // Scrollable
    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
    GtkScrollablePolicy hscroll_policy;
    GtkScrollablePolicy vscroll_policy;

    // Signals
    guint h_adj_signal;
    guint v_adj_signal;

    // Message Backlog
    std::optional<std::string> backlog_url;
};

G_DEFINE_FINAL_TYPE_WITH_CODE(WebContentView, web_content_view, GTK_TYPE_WIDGET,
                              G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

enum {
    PROP_0,
    PROP_HADJUSTMENT,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY,
    N_PROPS
};

//static GParamSpec *properties [N_PROPS];

GtkWidget *
web_content_view_new ()
{
    return (GtkWidget *) g_object_new (WEB_TYPE_CONTENT_VIEW,
                                       NULL);
}

static void
web_content_view_finalize (GObject *object)
{
//    WebContentView *self = (WebContentView *)object;

    G_OBJECT_CLASS (web_content_view_parent_class)->finalize (object);
}

static void
web_content_view_dispose (GObject *object)
{
//    WebContentView *self = (WebContentView *)object;

    G_OBJECT_CLASS (web_content_view_parent_class)->dispose (object);
}

static void
cb_adjustment_changed(WebContentView *self)
{
    if (self->view_impl.has_value()) {
        g_object_freeze_notify(G_OBJECT (self));
        self->view_impl->update_viewport_rect();
        g_object_thaw_notify(G_OBJECT (self));
    }
}

static void
web_content_view_get_property (GObject     *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    WebContentView *self = WEB_CONTENT_VIEW (object);

    switch (prop_id)
    {
        case PROP_HADJUSTMENT:
            g_value_set_object (value, self->hadjustment);
            break;
        case PROP_VADJUSTMENT:
            g_value_set_object (value, self->vadjustment);
            break;
        case PROP_HSCROLL_POLICY:
            g_value_set_enum (value, self->hscroll_policy);
            break;
        case PROP_VSCROLL_POLICY:
            g_value_set_enum (value, self->vscroll_policy);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
web_content_view_set_property (GObject       *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    WebContentView *self = WEB_CONTENT_VIEW (object);

    switch (prop_id)
    {
        case PROP_HADJUSTMENT:
            // Clear
            if (self->hadjustment) {
                g_signal_handler_disconnect(self->hadjustment, self->h_adj_signal);
                g_clear_object(&self->hadjustment);
            }

            // Set New
            self->hadjustment = GTK_ADJUSTMENT(g_value_get_object(value));

            // Connect
            if (self->hadjustment) {
                g_object_ref(self->hadjustment);
                self->h_adj_signal = g_signal_connect_swapped(self->hadjustment, "value-changed", G_CALLBACK(cb_adjustment_changed), self);
            }
            break;
        case PROP_VADJUSTMENT:
            // Clear
            if (self->vadjustment) {
                g_signal_handler_disconnect(self->vadjustment, self->v_adj_signal);
                g_clear_object(&self->vadjustment);
                gtk_widget_queue_resize(GTK_WIDGET (self));
            }

            // Set New
            self->vadjustment = GTK_ADJUSTMENT(g_value_get_object(value));

            // Connect
            if (self->vadjustment) {
                g_object_ref(self->vadjustment);
                self->v_adj_signal = g_signal_connect_swapped(self->vadjustment, "value-changed", G_CALLBACK(cb_adjustment_changed), self);
                gtk_widget_queue_resize(GTK_WIDGET (self));
            }
            break;
        case PROP_HSCROLL_POLICY:
            self->hscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
            break;
        case PROP_VSCROLL_POLICY:
            self->vscroll_policy = static_cast<GtkScrollablePolicy>(g_value_get_enum(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

void
web_content_view_load (WebContentView *self, const char *url)
{
    if (self->view_impl.has_value()) {
        self->view_impl->load(ak_string_from_cstring(url).value());
    } else {
        self->backlog_url = std::string(url);
    }
}

static void
web_content_view_snapshot(GtkWidget *self, GtkSnapshot *snapshot)
{
    if (WEB_CONTENT_VIEW(self)->view_impl.has_value()) {
        WEB_CONTENT_VIEW(self)->view_impl->snapshot_vfunc(snapshot);
    }
}

static void
web_content_view_size_allocate(GtkWidget *self, int width, int height, int baseline)
{
    if (WEB_CONTENT_VIEW(self)->view_impl.has_value()) {
        WEB_CONTENT_VIEW(self)->view_impl->size_allocate_vfunc(width, height, baseline);
    }
}

static void
web_content_view_class_init (WebContentViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = web_content_view_finalize;
    object_class->dispose = web_content_view_dispose;
    object_class->get_property = web_content_view_get_property;
    object_class->set_property = web_content_view_set_property;

    g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
    g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
    g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
    g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    widget_class->snapshot = web_content_view_snapshot;
    widget_class->size_allocate = web_content_view_size_allocate;
}

static void
cb_main_loop (WebContentView *self)
{
    // So... what's this for?
    //
    // Basically, the WebContentView widget needs the GTK Event Loop to already be running
    // otherwise the web process will crash. This then corrupts the widget and breaks everything.
    //
    // So we schedule the creation of the actual ViewImplementation (and thus WebContent process)
    // until we know the GLib event loop is running, which achieve with g_timeout_add_once().
    self->view_impl.emplace(g_object_ref(self), String(), WebView::EnableCallgrindProfiling::No, WebView::UseJavaScriptBytecode::Yes);

    if (self->backlog_url.has_value()) {
        web_content_view_load(WEB_CONTENT_VIEW(self), self->backlog_url->c_str());
        self->backlog_url.reset();
    }

    gtk_widget_queue_resize(GTK_WIDGET (self));
}

static void
on_realize (WebContentView *self) {
    // We need to wait for the type to have finished construction
    // This is a good a place as any
    g_timeout_add_once(0, reinterpret_cast<GSourceOnceFunc>(cb_main_loop), self);
}

static void
web_content_view_init (WebContentView *self)
{
    g_signal_connect(self, "realize", G_CALLBACK (on_realize), NULL);
}
