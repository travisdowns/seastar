/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2015 Cloudius Systems, Ltd.
 */

#include <functional>

#include <cstdio>

#include "alloc_test_outline.hh"


using sink_method = std::function<void()>;

using r_method = void(const unsigned depth,
        const unsigned max_depth, const unsigned path, const sink_method& m);

r_method* volatile method_sink;

r_method recurse_a;
r_method recurse_b;

void __attribute__((noinline)) recurse_a(const unsigned depth,
        const unsigned max_depth, const unsigned path, const sink_method& m) {
    if (depth >= max_depth) {
        m();
        return;
    }
    method_sink = (path >> depth) & 1U ? recurse_b : recurse_a;
    method_sink(depth + 1, max_depth, path, m);
    asm volatile (""); // disable tailcall
}

void __attribute__((noinline)) recurse_b(const unsigned depth,
        const unsigned max_depth, const unsigned path, const sink_method& m) {
    if (depth >= max_depth) {
        m();
        return;
    }
    method_sink = (path >> depth) & 1U ? recurse_b : recurse_a;
    method_sink(depth + 1, max_depth, path, m);
    asm volatile (""); // disable tailcall
}

void recurse_ab(unsigned depth, unsigned path, const sink_method& method) {
    recurse_a(0, depth, path, method);
}
