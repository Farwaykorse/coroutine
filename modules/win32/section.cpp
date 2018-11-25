﻿// ---------------------------------------------------------------------------
//
//  Author  : github.com/luncliff (luncliff@gmail.com)
//  License : CC BY 4.0
//
// ---------------------------------------------------------------------------

#define NOMINMAX

#include <coroutine/sync.h>
#include <system_error>

#include <Windows.h> // System API

static_assert(sizeof(section) == SYSTEM_CACHE_ALIGNMENT_SIZE);
static_assert(sizeof(CRITICAL_SECTION) <= sizeof(section));

section::section(uint16_t spin) noexcept(false)
{
    auto* section = reinterpret_cast<CRITICAL_SECTION*>(this->u64);
    // https://docs.microsoft.com/en-us/windows/desktop/api/synchapi/nf-synchapi-initializecriticalsectionandspincount
    ::InitializeCriticalSectionAndSpinCount(section, spin);
}

section::~section() noexcept
{
    auto* section = reinterpret_cast<CRITICAL_SECTION*>(this->u64);
    ::DeleteCriticalSection(section);
}

bool section::try_lock() noexcept
{
    auto* section = reinterpret_cast<CRITICAL_SECTION*>(this->u64);
    return ::TryEnterCriticalSection(section) == TRUE;
}

void section::lock() noexcept(false)
{
    auto* section = reinterpret_cast<CRITICAL_SECTION*>(this->u64);
    return ::EnterCriticalSection(section);
}

void section::unlock() noexcept(false)
{
    auto* section = reinterpret_cast<CRITICAL_SECTION*>(this->u64);
    return ::LeaveCriticalSection(section);
}
