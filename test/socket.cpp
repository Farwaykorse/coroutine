//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include "socket.h"

using namespace std;
using namespace coro;

#define _require_(cond)                                                        \
    if ((cond) == false) {                                                     \
        printf("%s %d\n", __FILE__, __LINE__);                                 \
        exit(__LINE__);                                                        \
    }
#define PRINT_MESSAGE(msg)                                                     \
    printf("%s %s %d\n", msg.c_str(), __FILE__, __LINE__);
#define FAIL_WITH_MESSAGE(msg)                                                 \
    {                                                                          \
        PRINT_MESSAGE(msg);                                                    \
        exit(__LINE__);                                                        \
    }
#define FAIL_WITH_CODE(ec) FAIL_WITH_MESSAGE(system_category().message(ec));

int64_t socket_create(const addrinfo& hint) {
    int64_t sd = ::socket(hint.ai_family, hint.ai_socktype, hint.ai_protocol);
    if (sd == -1) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
    return sd;
}

void socket_bind(int64_t sd, const sockaddr_in& local) {
    // bind socket and address
    if (::bind(sd, (const sockaddr*)&local, sizeof(sockaddr_in))) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
}
void socket_bind(int64_t sd, const sockaddr_in6& local) {
    // bind socket and address
    if (::bind(sd, (const sockaddr*)&local, sizeof(sockaddr_in6))) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
}

void socket_listen(int64_t sd) {
    if (::listen(sd, 7) != 0) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
}

int64_t socket_connect(int64_t sd, const sockaddr_in& remote) {
    if (::connect(sd, (const sockaddr*)&remote, sizeof(sockaddr_in)) < 0)
        return recent_net_error();
    return 0;
}
int64_t socket_connect(int64_t sd, const sockaddr_in6& remote) {
    if (::connect(sd, (const sockaddr*)&remote, sizeof(sockaddr_in6)) < 0)
        return recent_net_error();
    return 0;
}

int64_t socket_get_name(int64_t sd, sockaddr_in& local) {
    socklen_t len = sizeof(sockaddr_in);
    if (getsockname(gsl::narrow_cast<int64_t>(sd), (sockaddr*)&local, &len))
        return recent_net_error();
    return 0;
}

int64_t socket_get_name(int64_t sd, sockaddr_in6& local) {
    socklen_t len = sizeof(sockaddr_in6);
    if (getsockname(gsl::narrow_cast<int64_t>(sd), (sockaddr*)&local, &len))
        return recent_net_error();
    return 0;
}

int64_t socket_get_peer(int64_t sd, sockaddr_in& local) {
    socklen_t len = sizeof(sockaddr_in);
    if (getpeername(gsl::narrow_cast<int64_t>(sd), (sockaddr*)&local, &len))
        return recent_net_error();
    return 0;
}

int64_t socket_get_peer(int64_t sd, sockaddr_in6& local) {
    socklen_t len = sizeof(sockaddr_in6);
    if (getpeername(gsl::narrow_cast<int64_t>(sd), (sockaddr*)&local, &len))
        return recent_net_error();
    return 0;
}

int64_t socket_accept(int64_t ln) {
    return ::accept(ln, nullptr, nullptr);
}

void socket_set_option(int64_t sd, int64_t level, int64_t option,
                       int64_t value) {
    auto ec = ::setsockopt(sd, level, option, (char*)&value, sizeof(value));
    if (ec != 0) {
        ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
}

void socket_set_option_reuse_address(int64_t sd) {
    return socket_set_option(sd, SOL_SOCKET, SO_REUSEADDR, true);
}

void socket_set_option_nodelay(int64_t sd) {
    return socket_set_option(sd, IPPROTO_TCP, TCP_NODELAY, true);
}

void socket_set_option_timout(int64_t sd, uint32_t ms) {
    constexpr auto unit = 1000;
    timeval timeout{};
    timeout.tv_sec = ms / unit;
    timeout.tv_usec = (ms % unit) * unit;

    if (::setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, //
                     (char*)&timeout, sizeof(timeval)) != 0) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
    if (::setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, //
                     (char*)&timeout, sizeof(timeval)) != 0) {
        auto ec = recent_net_error();
        FAIL_WITH_CODE(ec);
    }
}

#if defined(_MSC_VER)

WSADATA wsa_data{};

void init_network_api() noexcept(false) {
    if (wsa_data.wVersion) // already initialized
        return;

    // init version 2.2
    if (::WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
        auto errc = WSAGetLastError();
        throw system_error{errc, system_category(), "WSAStartup"};
    }
}
void release_network_api() noexcept {
    // check and release
    if (wsa_data.wVersion != 0)
        ::WSACleanup();

    wsa_data.wVersion = 0;
}

int recent_net_error() noexcept {
    return WSAGetLastError();
}

bool is_in_progress(int ec) noexcept {
    if (ec == WSAEWOULDBLOCK || ec == EWOULDBLOCK || ec == EINPROGRESS ||
        ec == ERROR_IO_PENDING)
        return true;
    return false;
}
void socket_close(int64_t sd) {
    shutdown(sd, SD_BOTH);
    closesocket(sd);
}
void socket_set_option_nonblock(int64_t sd) {
    u_long mode = TRUE;
    _require_(ioctlsocket(sd, FIONBIO, &mode) == NO_ERROR);
}

#elif defined(__unix__) || defined(__linux__) || defined(__APPLE__)
void init_network_api() noexcept(false) {
    // do nothing for posix system. network operation already available
}
void release_network_api() noexcept {
}

int recent_net_error() noexcept {
    return errno;
}

bool is_in_progress(int ec) noexcept {
    return ec == EINPROGRESS;
}
void socket_close(int64_t sd) {
    shutdown(sd, SHUT_RDWR);
    close(sd);
}
void socket_set_option_nonblock(int64_t sd) {
    // make non-block/async
    _require_(fcntl(sd, F_SETFL, O_NONBLOCK) != -1);
}
#endif
