/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/String.h>
#include <glibmm/ustring.h>

AK::DeprecatedString ak_deprecated_string_from_cstring(const char *cstring);
ErrorOr<String> ak_string_from_cstring(const char *cstring);
ErrorOr<AK::DeprecatedString> ak_deprecated_string_from_ustring(Glib::ustring ustring);
ErrorOr<String> ak_string_from_ustring(Glib::ustring ustring);
Glib::ustring ustring_from_ak_deprecated_string(AK::DeprecatedString const& ak_deprecated_string);
Glib::ustring ustring_from_ak_string(String const& ak_string);
char* owned_cstring_from_ak_string(String const& ak_string);
void platform_init();

extern DeprecatedString s_serenity_resource_root;
