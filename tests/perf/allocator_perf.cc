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
 * Copyright (C) 2023 ScyllaDB
 */

#include <seastar/testing/perf_tests.hh>

#include <seastar/core/memory.hh>
#include <sys/mman.h>

struct alloc_bench {

    static constexpr size_t small_alloc_size = 8;
    static constexpr size_t large_alloc_size = 32000;
    static constexpr size_t MAX_POINTERS = 1000;
    
    enum alloc_test_flags : uint8_t {
        MEASURE_ALLOC = 1,
        MEASURE_FREE  = 2
    };

    using alloc_fn = void * (size_t);
    using free_fn = void (void *);

    struct c_funcs {
        static constexpr alloc_fn * const alloc = std::malloc;
        static constexpr free_fn * const free = std::free;
    };

    struct cpp_operator_funcs {
        static void * alloc(size_t size) { return operator new(size); }
        static void free(void *p){ operator delete(p); }
    };

    struct cpp_array_funcs {
        static void * alloc(size_t size) { return new char[size]; }
        static void free(void *p){ delete [] static_cast<char *>(p); }
    };

    // struct operator 

    static constexpr alloc_test_flags MEASURE_BOTH = (alloc_test_flags)(MEASURE_ALLOC | MEASURE_FREE);

    template <int M> struct maybe_measure {
        maybe_measure() {
          if constexpr (M)
            perf_tests::start_measuring_time();
        }
        ~maybe_measure() {
          if constexpr (M)
            perf_tests::stop_measuring_time();
        }
    };

    template <size_t alloc_size, alloc_test_flags F, typename A>
    size_t alloc_test() {
        {
          maybe_measure<F & MEASURE_ALLOC> m;
          for (auto &p : _pointers) {
            p = A::alloc(alloc_size);
          }
        }

        {
          maybe_measure<F & MEASURE_FREE> m;
          for (auto &p : _pointers) {
            A::free(p);
          }
        }

        return _pointers.size();
    }

    std::array<void *, MAX_POINTERS> _pointers{};
};

PERF_TEST_F(alloc_bench, malloc_only)      { return alloc_test<small_alloc_size, MEASURE_ALLOC, c_funcs>(); }
PERF_TEST_F(alloc_bench, free_only)        { return alloc_test<small_alloc_size, MEASURE_FREE, c_funcs>(); }
PERF_TEST_F(alloc_bench, malloc_free)      { return alloc_test<small_alloc_size, MEASURE_BOTH, c_funcs>(); }

PERF_TEST_F(alloc_bench, op_new_only)      { return alloc_test<small_alloc_size, MEASURE_ALLOC, cpp_operator_funcs>(); }
PERF_TEST_F(alloc_bench, op_delete_only)   { return alloc_test<small_alloc_size, MEASURE_FREE, cpp_operator_funcs>(); }
PERF_TEST_F(alloc_bench, op_new_delete)    { return alloc_test<small_alloc_size, MEASURE_BOTH, cpp_operator_funcs>(); }

// PERF_TEST_F(alloc_bench, new_array_only)   { return alloc_test<small_alloc_size, MEASURE_ALLOC, cpp_array_funcs>(); }
// PERF_TEST_F(alloc_bench, delete_array_only){ return alloc_test<small_alloc_size, MEASURE_FREE, cpp_array_funcs>(); }
// PERF_TEST_F(alloc_bench, new_delete_array) { return alloc_test<small_alloc_size, MEASURE_BOTH, cpp_array_funcs>(); }

PERF_TEST_F(alloc_bench, alloc_only_large) { return alloc_test<large_alloc_size, MEASURE_ALLOC, c_funcs>(); }
PERF_TEST_F(alloc_bench, free_only_large ) { return alloc_test<large_alloc_size, MEASURE_FREE, c_funcs>(); }
PERF_TEST_F(alloc_bench, alloc_free_large) { return alloc_test<large_alloc_size, MEASURE_BOTH, c_funcs>(); }

// this test doesn't serve much value. It should take about 10 times as the
// single alloc test above. If not, something is wrong.
PERF_TEST_F(alloc_bench, single_alloc_and_free_small_many)
{
    const std::size_t allocs = 10;
    static_assert(allocs <= MAX_POINTERS);

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(10);
        perf_tests::do_not_optimize(ptr);
        _pointers[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_pointers[i]);
    }
}

// Allocate from more than one page. Should also not suffer a perf penalty 
// As we should have at least min free of 50 objects (hard coded internally)
PERF_TEST_F(alloc_bench, single_alloc_and_free_small_many_cross_page)
{
    const std::size_t alloc_size = 1024;
    const std::size_t allocs = (seastar::memory::page_size / alloc_size) + 1;

    static_assert(allocs <= MAX_POINTERS);

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(alloc_size);
        perf_tests::do_not_optimize(ptr);
        _pointers[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_pointers[i]);
    }
}

// Include an allocation in the benchmark that will require going to the large pool
// for more data for the small pool
PERF_TEST_F(alloc_bench, single_alloc_and_free_small_many_cross_page_alloc_more)
{
    const std::size_t alloc_size = 1024;
    const std::size_t allocs = 101; // at 1024 alloc size we will have a _max_free of 100 objects

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(alloc_size);
        perf_tests::do_not_optimize(ptr);
        _pointers[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_pointers[i]);
    }
}
