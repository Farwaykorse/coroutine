//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include <coroutine/channel.hpp>
#include <coroutine/return.h>

#include "test.h"
using namespace std;
using namespace coro;

using channel_without_lock_t = channel<int>;

auto coro_channel_write_return_false_after_close_test() {
    auto ch = make_unique<channel_without_lock_t>();
    bool ok = true;

    auto coro_write = [&ok](auto& ch, auto value) -> preserve_frame {
        ok = co_await ch.write(value);
    };
    // coroutine will suspend and wait in the channel
    auto h = coro_write(*ch, int{});
    {
        auto truncator = move(ch); // if channel is destroyed ...
    }
    _require_(ch.get() == nullptr);

    coroutine_handle<void>& coro = h;
    _require_(coro.done()); // coroutine is in done state
    coro.destroy();         // destroy to prevent leak

    _require_(ok == false); // and channel returned false
    return EXIT_SUCCESS;
}

#if defined(CMAKE_TEST)
int main(int, char*[]) {
    return coro_channel_write_return_false_after_close_test();
}

#elif __has_include(<CppUnitTest.h>)
#include <CppUnitTest.h>

template <typename T>
using TestClass = ::Microsoft::VisualStudio::CppUnitTestFramework::TestClass<T>;

class coro_channel_write_return_false_after_close
    : public TestClass<coro_channel_write_return_false_after_close> {
    TEST_METHOD(test_coro_channel_write_return_false_after_close) {
        coro_channel_write_return_false_after_close_test();
    }
};
#endif
