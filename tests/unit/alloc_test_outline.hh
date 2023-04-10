#ifndef ALLOC_TEST_OUTLINE_H_
#define ALLOC_TEST_OUTLINE_H_

#include <functional>

using sink_method = std::function<void()>;

void recurse_ab(unsigned depth, unsigned path, const sink_method& method);

#endif
