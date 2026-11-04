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
 * Copyright (C) 2018 ScyllaDB Ltd.
 */

#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <boost/test/unit_test.hpp>
#include <seastar/testing/entry_point.hh>
#include <seastar/testing/seastar_test.hh>
#include <seastar/testing/test_runner.hh>

namespace seastar {

namespace testing {

static bool init_unit_test_suite() {
    auto&& ts = boost::unit_test::framework::master_test_suite();
    return global_test_runner().start(ts.argc, ts.argv);
}

static void dummy_handler(int) {
    // This handler should have been replaced.
    _exit(1);
}

static void install_dummy_handler(int sig) {
    struct sigaction sa {};
    sa.sa_handler = dummy_handler;
    sigaction(sig, &sa, nullptr);
}

// Boost.Test sizes its alternate signal stack as SIGSTKSZ. Where that is a
// compile-time constant smaller than the running kernel's minimum (musl on
// CPUs with AMX state), Boost's sigaltstack() call fails with ENOMEM and every
// test aborts at setup. In that case install a correctly sized stack before
// Boost starts, which makes Boost keep it for its init phase, and turn off
// Boost's per-test-case alternate stack, which would fail the same way.
static void work_around_boost_alt_stack_size() {
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

int entry_point(int argc, char** argv) {
    work_around_boost_alt_stack_size();
#ifndef SEASTAR_ASAN_ENABLED
    // Before we call into boost, install some dummy signal
    // handlers. This seems to be the only way to stop boost from
    // installing its own handlers, which disables our backtrace
    // printer. The real handler will be installed when the reactor is
    // constructed.
    // If we are using ASAN, it has already installed a signal handler
    // that does its own stack printing.
    for (int sig : {SIGSEGV, SIGABRT}) {
        install_dummy_handler(sig);
    }
#else
    (void)install_dummy_handler;
#endif

    const int boost_exit_code = ::boost::unit_test::unit_test_main(&init_unit_test_suite, argc, argv);
    const int seastar_exit_code = seastar::testing::global_test_runner().finalize();
    if (boost_exit_code) {
        return boost_exit_code;
    }
    return seastar_exit_code;
}

}

}
