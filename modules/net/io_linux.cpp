//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
#include <coroutine/net.h>

#include "event_poll.h"

static_assert(sizeof(ssize_t) <= sizeof(int64_t));
using namespace std;
using namespace std::chrono;

namespace coro {

event_poll_t inbound{}, outbound{};

auto enumerate_net_tasks(nanoseconds timeout) noexcept(false)
    -> coro::enumerable<io_task_t> {
    const int half_time = duration_cast<milliseconds>(timeout).count() / 2;
    io_task_t task{};

    for (auto event : inbound.wait(half_time))
        co_yield task = io_task_t::from_address(event.data.ptr);
    for (auto event : outbound.wait(half_time))
        co_yield task = io_task_t::from_address(event.data.ptr);
}
void wait_net_tasks(coro::enumerable<io_task_t>& tasks,
                    std::chrono::nanoseconds timeout) noexcept(false) {
    tasks = enumerate_net_tasks(timeout);
}

bool io_work_t::ready() const noexcept {
    auto sd = this->handle;
    // non blocking operation is expected
    // going to suspend
    if (fcntl(sd, F_GETFL, 0) & O_NONBLOCK)
        return false;

    // not configured. return true
    // and bypass to the blocking I/O
    return true;
}

uint32_t io_work_t::error() const noexcept {
    return gsl::narrow_cast<uint32_t>(this->internal);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

auto send_to(uint64_t sd, const sockaddr_in& remote, io_buffer_t buffer,
             io_work_t& work) noexcept(false) -> io_send_to& {
    work.handle = sd;
    work.ptr = const_cast<sockaddr_in*>(addressof(remote));
    work.internal_high = sizeof(sockaddr_in);
    work.buffer = buffer;
    return *reinterpret_cast<io_send_to*>(addressof(work));
}

auto send_to(uint64_t sd, const sockaddr_in6& remote, io_buffer_t buffer,
             io_work_t& work) noexcept(false) -> io_send_to& {
    work.handle = sd;
    work.ptr = const_cast<sockaddr_in6*>(addressof(remote));
    work.internal_high = sizeof(sockaddr_in6);
    work.buffer = buffer;
    return *reinterpret_cast<io_send_to*>(addressof(work));
}

void io_send_to::suspend(io_task_t rh) noexcept(false) {
    auto sd = this->handle;
    auto& errc = this->internal;
    errc = 0;

    epoll_event req{};
    req.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
    req.data.ptr = rh.address();

    outbound.try_add(sd, req); // throws if epoll_ctl fails
}

int64_t io_send_to::resume() noexcept {
    auto sd = this->handle;
    auto addr = reinterpret_cast<sockaddr*>(this->ptr);
    auto addrlen = static_cast<socklen_t>(this->internal_high);
    auto& errc = this->internal;
    auto sz = sendto(sd, buffer.data(), buffer.size_bytes(), //
                     0, addr, addrlen);
    // update error code upon i/o failure
    errc = sz < 0 ? errno : 0;
    return sz;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

auto recv_from(uint64_t sd, sockaddr_in& remote, io_buffer_t buffer,
               io_work_t& work) noexcept(false) -> io_recv_from& {
    work.handle = sd;
    work.ptr = addressof(remote);
    work.internal_high = sizeof(sockaddr_in);
    work.buffer = buffer;
    return *reinterpret_cast<io_recv_from*>(addressof(work));
}

auto recv_from(uint64_t sd, sockaddr_in6& remote, io_buffer_t buffer,
               io_work_t& work) noexcept(false) -> io_recv_from& {
    work.handle = sd;
    work.ptr = addressof(remote);
    work.internal_high = sizeof(sockaddr_in6);
    work.buffer = buffer;
    return *reinterpret_cast<io_recv_from*>(addressof(work));
}

void io_recv_from::suspend(io_task_t rh) noexcept(false) {
    auto sd = this->handle;
    auto& errc = this->internal;
    errc = 0;

    epoll_event req{};
    req.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
    req.data.ptr = rh.address();

    inbound.try_add(sd, req); // throws if epoll_ctl fails
}

int64_t io_recv_from::resume() noexcept {
    auto sd = this->handle;
    auto addr = reinterpret_cast<sockaddr*>(this->ptr);
    auto addrlen = static_cast<socklen_t>(this->internal_high);
    auto& errc = this->internal;
    auto sz = recvfrom(sd, buffer.data(), buffer.size_bytes(), //
                       0, addr, addressof(addrlen));
    // update error code upon i/o failure
    errc = sz < 0 ? errno : 0;
    return sz;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

auto send_stream(uint64_t sd, io_buffer_t buffer, uint32_t flag,
                 io_work_t& work) noexcept(false) -> io_send& {
    static_assert(sizeof(socklen_t) == sizeof(uint32_t));
    work.handle = sd;
    work.internal = flag;
    work.buffer = buffer;
    return *reinterpret_cast<io_send*>(addressof(work));
}

void io_send::suspend(io_task_t rh) noexcept(false) {
    auto sd = this->handle;
    auto& errc = this->internal;
    errc = 0;

    epoll_event req{};
    req.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
    req.data.ptr = rh.address();

    outbound.try_add(sd, req); // throws if epoll_ctl fails
}

int64_t io_send::resume() noexcept {
    auto sd = this->handle;
    auto flag = this->internal;
    auto& errc = this->internal;
    const auto sz = send(sd, buffer.data(), buffer.size_bytes(), flag);
    // update error code upon i/o failure
    errc = sz < 0 ? errno : 0;
    return sz;
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

auto recv_stream(uint64_t sd, io_buffer_t buffer, uint32_t flag,
                 io_work_t& work) noexcept(false) -> io_recv& {
    static_assert(sizeof(socklen_t) == sizeof(uint32_t));
    work.handle = sd;
    work.internal = flag;
    work.buffer = buffer;
    return *reinterpret_cast<io_recv*>(addressof(work));
}

void io_recv::suspend(io_task_t rh) noexcept(false) {
    auto sd = this->handle;
    auto& errc = this->internal;
    errc = 0;

    epoll_event req{};
    req.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
    req.data.ptr = rh.address();

    inbound.try_add(sd, req);
}

int64_t io_recv::resume() noexcept {
    auto sd = this->handle;
    auto flag = this->internal;
    auto& errc = this->internal;
    const auto sz = recv(sd, buffer.data(), buffer.size_bytes(), flag);
    // update error code upon i/o failure
    errc = sz < 0 ? errno : 0;
    return sz;
}

} // namespace coro
