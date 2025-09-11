#pragma once

#include <type_traits>
#include <utility>

template <class E>
class EnumRange 
{
    static_assert(std::is_enum_v<E>, "EnumRange requires an enum type.");
    using UT = std::underlying_type_t<E>;

public:
    // [first, last_inclusive]
    constexpr EnumRange(E first, E last_inclusive)
        : first_(first), last_exclusive_(next(last_inclusive)) {}

    // [first, last_exclusive)
    static constexpr EnumRange HalfOpen(E first, E last_exclusive) 
    {
        EnumRange r(first, first);
        r.last_exclusive_ = last_exclusive;
        return r;
    }

    // [E(0), Sentinel)  — e.g. Sentinel = E::COUNT
    template <E Sentinel>
    static constexpr EnumRange FromZeroToSentinel() 
    {
        return HalfOpen(static_cast<E>(0), Sentinel);
    }

    class iterator 
    {
    public:
        using value_type = E;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using reference = E;
        using pointer = void;

        constexpr explicit iterator(E v) : v_(v) {}
        constexpr E operator*() const { return v_; }
        constexpr iterator& operator++() { v_ = next(v_); return *this; }
        constexpr bool operator==(const iterator& other) const { return v_ == other.v_; }
        constexpr bool operator!=(const iterator& other) const { return !(*this == other); }

    private:
        E v_;
    };

    constexpr iterator begin() const { return iterator{ first_ }; }
    constexpr iterator end()   const { return iterator{ last_exclusive_ }; }

private:
    static constexpr E next(E e) 
    {
        return static_cast<E>(static_cast<UT>(e) + 1);
    }

    E first_;
    E last_exclusive_;
};

template <class E>
constexpr EnumRange<E> enum_range(E first, E last_inclusive) {
    return EnumRange<E>(first, last_inclusive);
}

template <class E>
constexpr EnumRange<E> enum_range_half_open(E first, E last_exclusive) {
    return EnumRange<E>::HalfOpen(first, last_exclusive);
}

template <class E, E Sentinel>
constexpr EnumRange<E> enum_range_to_sentinel() {
    return EnumRange<E>::template FromZeroToSentinel<Sentinel>();
}
