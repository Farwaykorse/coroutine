﻿//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include <coroutine/yield.hpp>

#include "test.h"
using namespace std;
using namespace coro;

auto yield_until_zero(int n) -> enumerable<int> {
    while (n-- > 0)
        co_yield n;
};
auto coro_enumerable_accumulate_test() {
    auto g = yield_until_zero(10);
    auto total = accumulate(g.begin(), g.end(), 0u);
    _require_(total == 45); // 0 - 10

    return EXIT_SUCCESS;
}

#if defined(CMAKE_TEST)
int main(int, char* []) {
    return coro_enumerable_accumulate_test();
}

#elif __has_include(<CppUnitTest.h>)
#include <CppUnitTest.h>

template <typename T>
using TestClass = ::Microsoft::VisualStudio::CppUnitTestFramework::TestClass<T>;

class coro_enumerable_accumulate
    : public TestClass<coro_enumerable_accumulate> {
    TEST_METHOD(test_coro_enumerable_accumulate) {
        coro_enumerable_accumulate_test();
    }
};
#endif
