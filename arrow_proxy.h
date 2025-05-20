#pragma once

// Helper proxy that lets an iterator yield a prvalue *pair* yet still
// offer operator-> (needed by some algorithms)
template<class Ref>
struct arrow_proxy {
    Ref r;
    constexpr Ref*       operator->()       noexcept { return &r; }
    constexpr Ref const* operator->() const noexcept { return &r; }
};

