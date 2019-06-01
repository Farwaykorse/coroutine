//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include "test_network.h"
#include "test_shared.h"

using namespace coro;

auto net_getaddrinfo_tcp6_listen_text_test() {
    init_network_api();
    auto on_return = gsl::finally([]() { release_network_api(); });

    addrinfo hint{};
    hint.ai_family = AF_INET6;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE | AI_V4MAPPED;

    size_t count = 0u;
    for (auto ep : resolve(hint, nullptr, "https")) {
        REQUIRE(ep.in6.sin6_family == AF_INET6);
        REQUIRE(ep.in6.sin6_port == htons(443));
        ++count;
    }
    REQUIRE(count > 0);
    return EXIT_SUCCESS;
}

#if defined(CMAKE_TEST)
int main(int, char* []) {
    return net_getaddrinfo_tcp6_listen_text_test();
}

#elif __has_include(<CppUnitTest.h>)
class net_getaddrinfo_tcp6_listen_text
    : public TestClass<net_getaddrinfo_tcp6_listen_text> {
    TEST_METHOD(test_net_getaddrinfo_tcp6_listen_text) {
        net_getaddrinfo_tcp6_listen_text_test();
    }
};
#endif
