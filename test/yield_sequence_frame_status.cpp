//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include <coroutine/return.h>
#include <coroutine/sequence.hpp>

#include "test.h"
using namespace std;
using namespace coro;

using status_t = int64_t;

auto sequence_suspend_and_return(coroutine_handle<void>& rh)
    -> sequence<status_t> {
    co_yield save_frame_t{rh};
    co_return;
}
auto use_sequence_suspend_and_return(status_t& ref, coroutine_handle<void>& rh)
    -> preserve_frame {
    // clang-format off
    for co_await(auto v : sequence_suspend_and_return(rh))
        ref = v;
    // clang-format on
    ref = 2; // change the value after iteration
};

auto coro_sequence_frame_status_test() {
    // notice that `sequence<status_t>` is placed in the caller's frame,
    //  therefore, when the caller is returned, sequence<status_t> will be
    // destroyed in caller coroutine frame's destruction phase
    preserve_frame fs{};
    //  automatically. so using `destroy` to the frame manually will lead to
    //  access violation ...

    status_t status = -1;
    // since there was no yield, it will remain unchanged
    auto fc = use_sequence_suspend_and_return(status, fs);
    auto on_return = gsl::finally([&fc]() {
        if (fc)
            fc.destroy();
    });
    _require_(status == -1);

    // however, when the sequence coroutine is resumed,
    // its caller will continue on ...
    _require_(fs.address() != nullptr);

    // sequence coroutine will be resumed
    //  and it will resume `use_sequence_suspend_and_return`.
    fs.resume();
    //  since `use_sequence_suspend_and_return` will co_return after that,
    //  both coroutine will reach *final suspended* state
    _require_(fc.done());
    _require_(fs.done());
    _require_(status == 2);

    return EXIT_SUCCESS;
}

#if defined(CMAKE_TEST)
int main(int, char* []) {
    return coro_sequence_frame_status_test();
}

#elif __has_include(<CppUnitTest.h>)
#include <CppUnitTest.h>

template <typename T>
using TestClass = ::Microsoft::VisualStudio::CppUnitTestFramework::TestClass<T>;

class coro_sequence_frame_status
    : public TestClass<coro_sequence_frame_status> {
    TEST_METHOD(test_coro_sequence_frame_status) {
        coro_sequence_frame_status_test();
    }
};
#endif
