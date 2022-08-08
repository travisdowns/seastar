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

#include <cstdlib>

#include <seastar/testing/perf_tests.hh>

#include <boost/lexical_cast.hpp>


template <typename T>
T from_env(const char* name, T default_val) {
    const char* val = getenv(name);
    return val ? boost::lexical_cast<T>(val) : default_val;
}

struct alloc_fixture {
    static constexpr size_t COUNT = 100000;

    size_t _alloc_size = from_env<size_t>("ALLOC_SIZE", 8);
    std::array<void *, COUNT> _pointers{};

    enum alloc_test_flags {
        MEASURE_ALLOC = 1 << 0,
        MEASURE_FREE  = 2 << 0,
    };

    template <int M>
    struct maybe_measure {
        maybe_measure() { if constexpr (M) perf_tests::start_measuring_time(); }
        ~maybe_measure() { if constexpr (M) perf_tests::stop_measuring_time(); }
    };

    template <alloc_test_flags F = alloc_test_flags(MEASURE_ALLOC | MEASURE_FREE)>
    size_t alloc_test() {

        auto alloc_size = _alloc_size;

        {
            maybe_measure<F & MEASURE_ALLOC> m;
            for (auto& p : _pointers) {
                p = std::malloc(alloc_size);
            }
        }

        {
            maybe_measure<F & MEASURE_FREE> m;
            for (auto& p : _pointers) {
                std::free(p);
            }
        }

        return _pointers.size();
    }

};



PERF_TEST_F(alloc_fixture, alloc_only)
{
    return alloc_test<MEASURE_ALLOC>();
}

PERF_TEST_F(alloc_fixture, free_only)
{
    return alloc_test<MEASURE_FREE>();
}

PERF_TEST_F(alloc_fixture, alloc_free)
{
    return alloc_test();
}

