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

// Linked into tests that use Boost.Test's own main(). Boost.Test sizes its
// alternate signal stack as SIGSTKSZ. Where that is a compile-time constant
// smaller than the running kernel's minimum (musl on CPUs with AMX state),
// Boost's sigaltstack() call fails with ENOMEM and every test aborts at setup.
// In that case install a correctly sized stack before main(), which makes
// Boost keep it for its init phase, and turn off Boost's per-test-case
// alternate stack, which would fail the same way.

#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace {

[[gnu::constructor]] void work_around_boost_alt_stack_size() {
#ifdef _SC_SIGSTKSZ
    long needed = sysconf(_SC_SIGSTKSZ);
    if (needed <= 0 || static_cast<size_t>(needed) <= static_cast<size_t>(SIGSTKSZ)) {
        return;
    }
    stack_t st {};
    st.ss_sp = new char[needed]; // lives for the rest of the process
    st.ss_size = needed;
    sigaltstack(&st, nullptr);
    setenv("BOOST_TEST_USE_ALT_STACK", "no", 0);
#endif
}

}
