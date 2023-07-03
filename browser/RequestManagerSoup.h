/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Loader/ResourceLoader.h>
#include <glibmm/object.h>

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

    sigc::signal<void(int)> signal_reply_finished;

private:
    RequestManagerSoup();

    class Request
        : public Web::ResourceLoaderConnectorRequest {
    public:
        static ErrorOr<NonnullRefPtr<Request>> create(DeprecatedString const& method, AK::URL const& url, HashMap<DeprecatedString, DeprecatedString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&);

        virtual ~Request() override;

        virtual void set_should_buffer_all_input(bool) override { }
        virtual bool stop() override { return false; }
        virtual void stream_into(Stream&) override { }

        void did_finish();

//        QNetworkReply& reply() { return m_reply; }

    private:
        Request(int);
//
//        QNetworkReply& m_reply;
    };

//    HashMap<QNetworkReply*, NonnullRefPtr<Request>> m_pending;
//    QNetworkAccessManager* m_qnam { nullptr };
};
