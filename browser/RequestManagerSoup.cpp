/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "RequestManagerSoup.h"
#include "Utilities.h"
#include <AK/JsonObject.h>

RequestManagerSoup::RequestManagerSoup()
{
    m_session = soup_session_new();
}

void RequestManagerSoup::reply_finished(SoupSession* session, GAsyncResult* result, gpointer user_data)
{
    auto *self = static_cast<RequestManagerSoup *>(user_data);
    SoupMessage *reply = soup_session_get_async_result_message(session, result);
    auto request = self->m_pending.get(reply).value();
    self->m_pending.remove(reply);
    request->did_finish(session, reply, result);
}

RefPtr<Web::ResourceLoaderConnectorRequest> RequestManagerSoup::start_request(DeprecatedString const& method, AK::URL const& url, HashMap<DeprecatedString, DeprecatedString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const& proxy)
{
    if (!url.scheme().is_one_of_ignoring_ascii_case("http"sv, "https"sv)) {
        return nullptr;
    }
    auto request_or_error = create_request(m_session, method, url, request_headers, request_body, proxy);
    if (request_or_error.is_error()) {
        return nullptr;
    }
    auto request = request_or_error.release_value();
    m_pending.set(request->reply(), *request);
    return request;
}

ErrorOr<NonnullRefPtr<RequestManagerSoup::Request>> RequestManagerSoup::create_request(SoupSession *session, DeprecatedString const& method, AK::URL const& url, HashMap<DeprecatedString, DeprecatedString> const& request_headers, ReadonlyBytes request_body, Core::ProxyData const&)
{
    SoupMessageHeaders *soup_request_headers;
    SoupMessage *msg;

    soup_request_headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_REQUEST);

    /* Initialize URL and headers */
    for (auto& it : request_headers) {
        if (!g_ascii_strcasecmp(it.key.characters(), "Accept-Encoding"))
            continue;
        soup_message_headers_append(soup_request_headers, it.key.characters(), it.value.characters());
    }

    /* NOTE: We explicitly disable HTTP2 as it's significantly slower (up to 5x, possibly more) */
    // TODO: How to do this in Soup?

    char *c_url = owned_cstring_from_ak_string(url.to_string().value());

    if (method.equals_ignoring_ascii_case("head"sv)) {
        msg = soup_message_new (SOUP_METHOD_HEAD, c_url);
    } else if (method.equals_ignoring_ascii_case("get"sv)) {
        msg = soup_message_new (SOUP_METHOD_GET, c_url);
    } else if (method.equals_ignoring_ascii_case("post"sv)) {
        msg = soup_message_new (SOUP_METHOD_POST, c_url);

        GBytes *request_bytes = g_bytes_new_static ((char const*)request_body.data(), request_body.size());
        soup_message_set_request_body_from_bytes(msg, "application/octet-stream", request_bytes);
        g_bytes_unref (request_bytes);
    } else if (method.equals_ignoring_ascii_case("put"sv)) {
        msg = soup_message_new (SOUP_METHOD_PUT, c_url);

        GBytes *request_bytes = g_bytes_new_static ((char const*)request_body.data(), request_body.size());
        soup_message_set_request_body_from_bytes(msg, "application/octet-stream", request_bytes);
        g_bytes_unref (request_bytes);
    } else if (method.equals_ignoring_ascii_case("delete"sv)) {
        msg = soup_message_new (SOUP_METHOD_DELETE, c_url);
    } else {
        // Custom e.g. for HTTP OPTIONS
        // Do we need this?
        msg = soup_message_new (method.characters(), c_url);
        GBytes *request_bytes = g_bytes_new_static ((char const*)request_body.data(), request_body.size());
        soup_message_set_request_body_from_bytes(msg, "application/octet-stream", request_bytes);
        g_bytes_unref (request_bytes);
    }

    g_free(c_url);

    soup_session_send_and_read_async (
            session,
            msg,
            G_PRIORITY_DEFAULT,
            nullptr,
            reinterpret_cast<GAsyncReadyCallback>(reply_finished),
            this);

    return adopt_ref (*new Request(msg));
}

RequestManagerSoup::Request::Request(SoupMessage *reply)
    : m_reply(reply)
{
}

RequestManagerSoup::Request::~Request() = default;

void RequestManagerSoup::Request::did_finish(SoupSession *session, SoupMessage *reply, GAsyncResult *result)
{
    GError *error = nullptr;
    GBytes *buffer = soup_session_send_and_read_finish(session, result, &error);

    if (error) {
        dbgln("Request Error: {}", error->message);
        g_error_free(error);
        return;
    }

    auto http_status_code = soup_message_get_status(reply);
    auto http_response_headers = soup_message_get_response_headers(reply);
    HashMap<DeprecatedString, DeprecatedString, CaseInsensitiveStringTraits> response_headers;
    Vector<DeprecatedString> set_cookie_headers;

    SoupMessageHeadersIter iter;
    const char *c_name, *c_value;

    soup_message_headers_iter_init(&iter, http_response_headers);
    while (soup_message_headers_iter_next(&iter, &c_name, &c_value))
    {
        auto name = DeprecatedString(c_name);
        auto value = DeprecatedString(c_value);
        if (name.equals_ignoring_ascii_case("set-cookie"sv)) {
            set_cookie_headers.append(value);
        } else {
            response_headers.set(name, value);
        }
    }

    if (!set_cookie_headers.is_empty()) {
        response_headers.set("set-cookie"sv, JsonArray { set_cookie_headers }.to_deprecated_string());
    }
    bool success = http_status_code != 0;
    gsize buffer_length;
    auto buffer_data = g_bytes_get_data(buffer, &buffer_length);
    on_buffered_request_finish(success, buffer_length, response_headers, http_status_code, ReadonlyBytes { buffer_data, (size_t)buffer_length });
    g_bytes_unref(buffer);
}
