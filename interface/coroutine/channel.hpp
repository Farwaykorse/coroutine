//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//  Note
//      Coroutine based channel
//      This is a simplified form of channel in The Go Language
//
#pragma once
#ifndef LUNCLIFF_COROUTINE_CHANNEL_HPP
#define LUNCLIFF_COROUTINE_CHANNEL_HPP

#if __has_include(<coroutine/frame.h>)
#include <coroutine/frame.h>
#elif __has_include(<experimental/coroutine>) // C++ 17
#include <experimental/coroutine>
#elif __has_include(<coroutine>) // C++ 20
#include <coroutine>
#else
#error "expect header <experimental/coroutine> or <coroutine/frame.h>"
#endif
#include <mutex>
#include <tuple>

namespace coro {
using namespace std;
using namespace std::experimental;

// Lockable without lock operation.
struct bypass_lock final {
    constexpr bool try_lock() noexcept {
        return true;
    }
    constexpr void lock() noexcept {
        // do nothing since this is 'bypass' lock
    }
    constexpr void unlock() noexcept {
        // it is not locked
    }
};

namespace internal {

// Linked list without allocation
template <typename HasNextPtr>
class list {
    using node_type = HasNextPtr;

    node_type* head{};
    node_type* tail{};

  public:
    list() noexcept = default;

  public:
    bool is_empty() const noexcept(false) {
        return head == nullptr;
    }
    void push(node_type* node) noexcept(false) {
        if (tail) {
            tail->next = node;
            tail = node;
        } else
            head = tail = node;
    }
    auto pop() noexcept(false) -> node_type* {
        node_type* node = head;
        if (head == tail) // empty or 1
            head = tail = nullptr;
        else // 2 or more
            head = head->next;

        return node; // this can be nullptr
    }
};
} // namespace internal

template <typename T, typename M = bypass_lock>
class channel; // by default, channel doesn't care about the race condition
template <typename T, typename M>
class reader;
template <typename T, typename M>
class writer;

// Awaitable for channel's read operation
template <typename T, typename M>
class reader final {
  public:
    using value_type = T;
    using pointer = value_type*;
    using reference = value_type&;
    using channel_type = channel<value_type, M>;

  private:
    using reader_list = typename channel_type::reader_list;
    using writer = typename channel_type::writer;
    using writer_list = typename channel_type::writer_list;

    friend channel_type;
    friend writer;
    friend reader_list;

  private:
    mutable pointer ptr;                  // address of the object
    mutable coroutine_handle<void> frame; // reader/writer coroutine's frame
    union {
        reader* next = nullptr; // Next reader in channel
        channel_type* chan;     // Channel to push this reader
    };

  private:
    explicit reader(channel_type& ch) noexcept(false)
        : ptr{}, frame{nullptr}, chan{addressof(ch)} {
    }
    reader(const reader&) noexcept = delete;
    reader& operator=(const reader&) noexcept = delete;
    reader(reader&&) noexcept = delete;
    reader& operator=(reader&&) noexcept = delete;

  public:
    ~reader() noexcept = default;

  public:
    void unlock_anyway() const noexcept(false) {
        chan->mtx.unlock();
    }

  private:
    auto match_after_lock() const noexcept(false) -> writer* {
        chan->mtx.lock();
        if (chan->writer_list::is_empty())
            // await_suspend will unlock in the case
            return nullptr;
        return chan->writer_list::pop();
    }

    void exchange_then_unlock(writer* w) const noexcept(false) {
        // exchange address & resumeable_handle
        swap(this->ptr, w->ptr);
        swap(this->frame, w->frame);
        return unlock_anyway();
    }

    void push_then_unlock(channel_type& ch,
                          coroutine_handle<void> coro) noexcept(false) {
        this->frame = coro;         // remember the handle before push
        this->next = nullptr;       // clear to prevent confusing
        ch.reader_list::push(this); // push to the channel
        return ch.mtx.unlock();
    }

    bool try_fetch_and_resume(reference storage) noexcept(false) {
        // frame holds poision if the channel is going to be destroyed
        if (this->frame == noop_coroutine())
            return false;
        // Store first. we have to do this because the resume operation
        // can destroy the writer coroutine
        storage = move(*this->ptr);
        if (this->frame)
            this->frame.resume();
        return true;
    }

  public:
    bool await_ready() const noexcept(false) {
        if (writer* w = match_after_lock()) {
            exchange_then_unlock(w);
            return true;
        }
        return false;
    }

    void await_suspend(coroutine_handle<void> coro) noexcept(false) {
        // notice that next & chan are sharing memory
        channel_type& ch = *(this->chan);
        return push_then_unlock(ch, coro);
    }

    auto await_resume() noexcept(false) -> tuple<value_type, bool> {
        auto t = make_tuple(value_type{}, false);
        get<1>(t) = try_fetch_and_resume(get<0>(t));
        return t;
    }
};

// Awaitable for channel's write operation
template <typename T, typename M>
class writer final {
  public:
    using value_type = T;
    using pointer = T*;
    using channel_type = channel<T, M>;

  private:
    using reader = typename channel_type::reader;
    using reader_list = typename channel_type::reader_list;
    using writer_list = typename channel_type::writer_list;

    friend channel_type;
    friend reader;
    friend writer_list;

  private:
    mutable pointer ptr;                  // address of the object
    mutable coroutine_handle<void> frame; // reader/writer coroutine's frame
    union {
        writer* next = nullptr; // Next writer in channel
        channel_type* chan;     // Channel to push this writer
    };

  private:
    explicit writer(channel_type& ch, pointer pv) noexcept(false)
        : ptr{pv}, frame{nullptr}, chan{addressof(ch)} {
    }
    writer(const writer&) noexcept = delete;
    writer& operator=(const writer&) noexcept = delete;
    writer(writer&&) noexcept = delete;
    writer& operator=(writer&&) noexcept = delete;

  public:
    ~writer() noexcept = default;

  private:
    void unlock_anyway() const noexcept(false) {
        chan->mtx.unlock();
    }

    auto match_after_lock() const noexcept(false) -> reader* {
        chan->mtx.lock();
        if (chan->reader_list::is_empty())
            // await_suspend will unlock in the case
            return nullptr;
        return chan->reader_list::pop();
    }

    void exchange_then_unlock(reader* r) const noexcept(false) {
        // exchange address & resumeable_handle
        swap(this->ptr, r->ptr);
        swap(this->frame, r->frame);
        return unlock_anyway();
    }

    void push_then_unlock(channel_type& ch,
                          coroutine_handle<void> coro) noexcept(false) {
        this->frame = coro;         // remember the handle before push
        this->next = nullptr;       // clear to prevent confusing
        ch.writer_list::push(this); // push to the channel
        return ch.mtx.unlock();
    }

    bool try_resume() noexcept(false) {
        // frame holds poision if the channel is going to be destroyed
        if (this->frame == noop_coroutine())
            return false;
        if (this->frame)
            this->frame.resume();
        return true;
    }

  public:
    bool await_ready() const noexcept(false) {
        if (reader* r = match_after_lock()) {
            exchange_then_unlock(r);
            return true;
        }
        return false;
    }

    void await_suspend(coroutine_handle<void> coro) noexcept(false) {
        // notice that next & chan are sharing memory
        channel_type& ch = *(this->chan);
        push_then_unlock(ch, coro);
    }

    bool await_resume() noexcept(false) {
        return try_resume();
    }
};

// Coroutine based channel. User have to provide appropriate lockable
template <typename T, typename M>
class channel final : internal::list<reader<T, M>>,
                      internal::list<writer<T, M>> {
    static_assert(is_reference<T>::value == false,
                  "reference type can't be channel's value_type.");

  public:
    using value_type = T;
    using pointer = value_type*;
    using reference = value_type&;
    using mutex_type = M;

  public:
    using reader = reader<value_type, mutex_type>;
    using writer = writer<value_type, mutex_type>;

  private:
    using reader_list = internal::list<reader>;
    using writer_list = internal::list<writer>;

    friend reader;
    friend writer;

  private:
    mutex_type mtx{};

  private:
    channel(const channel&) noexcept(false) = delete;
    channel(channel&&) noexcept(false) = delete;
    channel& operator=(const channel&) noexcept(false) = delete;
    channel& operator=(channel&&) noexcept(false) = delete;

  public:
    channel() noexcept(false) : reader_list{}, writer_list{}, mtx{} {
        // initialized 2 linked list and given mutex
    }
    ~channel() noexcept(false) {
        writer_list& writers = *this;
        reader_list& readers = *this;
        //
        // If the channel is raced hardly, some coroutines can be
        //  enqueued into list just after this destructor unlocks mutex.
        //
        // Unfortunately, this can't be detected at once since
        //  we have 2 list (readers/writers) in the channel.
        //
        // Current implementation allows checking repeatedly to reduce the
        //  probability of such interleaving.
        // Increase the repeat count below if the situation occurs.
        // But notice that it is NOT zero.
        //
        size_t repeat = 1; // author experienced 5'000+ for hazard usage
        while (repeat--) {
            unique_lock lck{mtx};
            while (writers.is_empty() == false) {
                writer* w = writers.pop();
                auto coro = w->frame;
                w->frame = noop_coroutine();

                coro.resume();
            }
            while (readers.is_empty() == false) {
                reader* r = readers.pop();
                auto coro = r->frame;
                r->frame = noop_coroutine();

                coro.resume();
            }
        }
    }

  public:
    decltype(auto) write(reference ref) noexcept(false) {
        return writer{*this, addressof(ref)};
    }
    decltype(auto) read() noexcept(false) {
        return reader{*this};
    }
};

//  If the channel is readable, acquire the value and the function.
template <typename T, typename M, typename Fn>
void select(channel<T, M>& ch, Fn&& fn) noexcept(false) {
    using channel_t = channel<T, M>;
    using reader_t = typename channel_t::reader;

    reader_t r = ch.read();
    if (r.await_ready()) { // if false, it's locked. we must ensure unlock
        auto tup = r.await_resume(); // move the value to current stack
        if (bool acquired = get<1>(tup); acquired)
            fn(get<0>(tup));
        return;
    }
    return r.unlock_anyway(); // work just like scoped locking
}

//  Invoke `select` for each pairs (channel + function)
template <typename... Args, typename ChanType, typename FuncType>
void select(ChanType& ch, FuncType&& fn, Args&&... args) noexcept(false) {
    select(ch, forward<FuncType&&>(fn));     // evaluate
    return select(forward<Args&&>(args)...); // try next pair
}

} // namespace coro

#endif // LUNCLIFF_COROUTINE_CHANNEL_HPP
