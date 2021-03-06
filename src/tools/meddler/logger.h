//
// Part of Metta OS. Check https://atta-metta.net for latest version.
//
// Copyright 2007 - 2017, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "config.h"

#if TOOLS_DEBUG && MEDDLER_DEBUG
#define L(...) __VA_ARGS__
#else
#define L(...)
#endif // TOOLS_DEBUG && MEDDLER_DEBUG
