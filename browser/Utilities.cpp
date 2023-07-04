/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Utilities.h"
#include <AK/LexicalPath.h>
#include <AK/Platform.h>
#include <LibFileSystem/FileSystem.h>

DeprecatedString s_serenity_resource_root;

AK::DeprecatedString ak_deprecated_string_from_cstring(const char *cstring)
{
    return AK::DeprecatedString(cstring);
}

ErrorOr<String> ak_string_from_cstring(const char *cstring)
{
    return String::from_utf8(StringView(cstring, strlen(cstring)));
}

Glib::ustring ustring_from_ak_deprecated_string(AK::DeprecatedString const& ak_deprecated_string)
{
    return {ak_deprecated_string.characters(), ak_deprecated_string.length()};
}

Glib::ustring ustring_from_ak_string(String const& ak_string)
{
    auto view = ak_string.bytes_as_string_view();
    return {view.characters_without_null_termination(), view.length()};
}

char* owned_cstring_from_ak_string(String const& ak_string)
{
    auto view = ak_string.bytes_as_string_view();
    return g_strndup(view.characters_without_null_termination(), view.length());
}

void platform_init()
{
#ifdef AK_OS_ANDROID
    extern void android_platform_init();
    android_platform_init();
#else
    s_serenity_resource_root = [] {
        auto const* source_dir = getenv("SERENITY_SOURCE_DIR");
        if (source_dir) {
            return DeprecatedString::formatted("{}/Base", source_dir);
        }
        auto* home = getenv("XDG_CONFIG_HOME") ?: getenv("HOME");
                VERIFY(home);
        auto home_lagom = DeprecatedString::formatted("{}/.lagom", home);
        if (FileSystem::is_directory(home_lagom))
            return home_lagom;
        auto app_dir = ak_deprecated_string_from_cstring(g_get_current_dir());
#    ifdef AK_OS_MACOS
        return LexicalPath(app_dir).parent().append("Resources"sv).string();
#    else
        return LexicalPath(app_dir).parent().append("share"sv).string();
#    endif
    }();
#endif
}
