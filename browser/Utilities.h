/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/String.h>
#include <glibmm/ustring.h>

AK::DeprecatedString ak_deprecated_string_from_cstring(const char*);
ErrorOr<String> ak_string_from_cstring(const char*);
Glib::ustring ustring_from_ak_deprecated_string(AK::DeprecatedString const&);
Glib::ustring ustring_from_ak_string(String const&);
char* owned_cstring_from_ak_string(String const& ak_string);
void platform_init();

extern DeprecatedString s_serenity_resource_root;
