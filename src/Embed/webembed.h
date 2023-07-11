/*
 * Copyright (c) 2023, Matthew Jakeman <mattjakemandev@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "AK/Error.h"
#include <LibCore/EventLoop.h>
#include <memory>

std::unique_ptr<Core::EventLoop> event_loop_ptr;

void webembed_init();