/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Loader/ResourceLoader.h>
#include <glibmm/object.h>
#include <libsoup/soup.h>

class RequestManagerSoup
    : Glib::Object
    , public Web::ResourceLoaderConnector {
public:
    static NonnullRefPtr<RequestManagerSoup> create()
    {
        return adopt_ref(*new RequestManagerSoup());
    }

    virtual ~RequestManagerSoup() override { }

    virtual void prefetch_dns(AK::URL const&) override { }
    virtual void preconnect(AK::URL const&) override { }

    virtual RefPtr<Web::ResourceLoaderConnectorRequest> start_request(DeprecatedString const& method, AK::URL const&, HashMap<DeprecatedString, DeprecatedString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&) override;

private:
    RequestManagerSoup();

    class Request
        : public Web::ResourceLoaderConnectorRequest {
    friend RequestManagerSoup;
    public:
        virtual ~Request() override;

        virtual void set_should_buffer_all_input(bool) override { }
        virtual bool stop() override { return false; }
        virtual void stream_into(Stream&) override { }

        void did_finish(SoupSession *session, SoupMessage *reply, GAsyncResult *result);

        SoupMessage *reply() { return m_reply; }

    private:
        explicit Request(SoupMessage *message);

        SoupMessage *m_reply;
    };

    ErrorOr<NonnullRefPtr<RequestManagerSoup::Request>> create_request(SoupSession *session, DeprecatedString const& method, AK::URL const& url, HashMap<DeprecatedString, DeprecatedString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&);
    static void reply_finished(SoupSession* session, GAsyncResult* result, gpointer);

    HashMap<SoupMessage*, NonnullRefPtr<Request>> m_pending;
    SoupSession* m_session;
};
