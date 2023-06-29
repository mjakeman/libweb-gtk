/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/String.h>
//#include <QString>
//
AK::DeprecatedString ak_deprecated_string_from_cstring(const char*);
ErrorOr<String> ak_string_from_cstring(const char*);
//QString qstring_from_ak_deprecated_string(AK::DeprecatedString const&);
//QString qstring_from_ak_string(String const&);
void platform_init();

extern DeprecatedString s_serenity_resource_root;
