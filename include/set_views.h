#pragma once

#include <ranges>
#include <functional>
#include "arrow_proxy.h"

/// @brief Set operation enumeration used by set_view.
enum class set_op { overlap, subtract, merge, exclusive };

namespace detail {

    /// @brief Iterator implementing lazy set algorithms.
    template<set_op Op, std::forward_iterator I1, std::sentinel_for<I1> S1,
             std::forward_iterator I2, std::sentinel_for<I2> S2, typename Comp>
    class set_iter {
    public:
        using value_type = std::remove_cvref_t<std::iter_reference_t<I1>>;
        using reference  = std::iter_reference_t<I1>;
        using pointer    = arrow_proxy<reference>;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::forward_iterator_tag;

        /// @brief Construct iterator with range state.
        set_iter(I1 it1, S1 end1, I2 it2, S2 end2, const Comp* comp)
            : it1_(it1), end1_(end1), it2_(it2), end2_(end2), comp_(comp)
        { satisfy(); }

        /// @brief Dereference current element.
        reference operator*() const { return use_first_ ? *it1_ : *it2_; }
        /// @brief Arrow operator for structured bindings.
        pointer operator->() const { return pointer{**this}; }

        /// @brief Pre-increment iterator.
        set_iter& operator++()
        {
            advance();
            satisfy();
            return *this;
        }
        /// @brief Post-increment iterator.
        set_iter operator++(int) { auto t = *this; ++(*this); return t; }

        /// @brief Equality comparison with sentinel.
        bool operator==(std::default_sentinel_t) const
        {
            if constexpr (Op == set_op::overlap || Op == set_op::subtract)
                return it1_ == end1_;
            else
                return it1_ == end1_ && it2_ == end2_;
        }

    private:
        void advance()
        {
            if constexpr (Op == set_op::overlap) {
                ++it1_; ++it2_;
            } else if constexpr (Op == set_op::subtract) {
                ++it1_;
            } else if constexpr (Op == set_op::merge) {
                if (it1_ == end1_) { ++it2_; }
                else if (it2_ == end2_) { ++it1_; }
                else if ((*comp_)(*it1_, *it2_)) { ++it1_; }
                else if ((*comp_)(*it2_, *it1_)) { ++it2_; }
                else { ++it1_; ++it2_; }
            } else { // exclusive
                if (use_first_) ++it1_; else ++it2_;
            }
        }

        void satisfy()
        {
            if constexpr (Op == set_op::overlap) {
                while (it1_ != end1_ && it2_ != end2_) {
                    if ((*comp_)(*it1_, *it2_)) ++it1_;
                    else if ((*comp_)(*it2_, *it1_)) ++it2_;
                    else break;
                }
                use_first_ = true;
            } else if constexpr (Op == set_op::subtract) {
                while (it1_ != end1_ && it2_ != end2_) {
                    if ((*comp_)(*it1_, *it2_)) break;
                    if ((*comp_)(*it2_, *it1_)) ++it2_;
                    else { ++it1_; ++it2_; }
                }
                use_first_ = true;
            } else if constexpr (Op == set_op::merge) {
                if (it1_ == end1_) { use_first_ = false; return; }
                if (it2_ == end2_) { use_first_ = true; return; }
                if ((*comp_)(*it1_, *it2_)) use_first_ = true;
                else if ((*comp_)(*it2_, *it1_)) use_first_ = false;
                else use_first_ = true;
            } else { // exclusive
                while (true) {
                    if (it1_ == end1_) { use_first_ = false; break; }
                    if (it2_ == end2_) { use_first_ = true; break; }
                    if ((*comp_)(*it1_, *it2_)) { use_first_ = true; break; }
                    if ((*comp_)(*it2_, *it1_)) { use_first_ = false; break; }
                    ++it1_; ++it2_;
                }
            }
        }

        I1  it1_{}; ///< @brief Iterator into first range.
        S1  end1_{}; ///< @brief Sentinel for first range.
        I2  it2_{}; ///< @brief Iterator into second range.
        S2  end2_{}; ///< @brief Sentinel for second range.
        const Comp* comp_{nullptr}; ///< @brief Pointer to comparator.
        bool use_first_{true}; ///< @brief Which iterator provides current value.
    };

    /// @brief Helper view storing two ranges for set operations.
    template<set_op Op, std::ranges::forward_range R1, std::ranges::forward_range R2, typename Comp>
    class set_view : public std::ranges::view_interface<set_view<Op,R1,R2,Comp>> {
        using I1 = std::ranges::iterator_t<R1>;
        using S1 = std::ranges::sentinel_t<R1>;
        using I2 = std::ranges::iterator_t<R2>;
        using S2 = std::ranges::sentinel_t<R2>;
    public:
        /// @brief Construct view from ranges and comparator.
        set_view(R1 r1, R2 r2, Comp comp)
            : r1_(std::move(r1)), r2_(std::move(r2)), comp_(std::move(comp)) {}

        /// @brief Iterator to first element.
        auto begin()
        {
            return set_iter<Op,I1,S1,I2,S2,Comp>{std::ranges::begin(r1_), std::ranges::end(r1_),
                                                std::ranges::begin(r2_), std::ranges::end(r2_), &comp_};
        }
        /// @brief End sentinel.
        std::default_sentinel_t end() { return {}; }
    private:
        R1   r1_; ///< @brief First range.
        R2   r2_; ///< @brief Second range.
        Comp comp_; ///< @brief Comparator instance.
    };

    template<set_op Op, typename R2, typename Comp>
    struct binder {
        R2 r2; ///< @brief Stored second range.
        Comp comp; ///< @brief Stored comparator.
        template<std::ranges::forward_range R1>
        friend auto operator|(R1&& r1, binder b)
        {
            return set_view<Op, std::views::all_t<R1>, R2, Comp>(std::views::all(std::forward<R1>(r1)), b.r2, b.comp);
        }
    };

    template<set_op Op>
    struct view_adaptor {
        template<std::ranges::forward_range R1, std::ranges::forward_range R2, typename Comp>
        auto operator()(R1&& r1, R2&& r2, Comp comp) const
        {
            return set_view<Op, std::views::all_t<R1>, std::views::all_t<R2>, Comp>(
                std::views::all(std::forward<R1>(r1)),
                std::views::all(std::forward<R2>(r2)), comp);
        }
        template<std::ranges::forward_range R2, typename Comp>
        auto operator()(R2&& r2, Comp comp) const
        {
            return binder<Op, std::views::all_t<R2>, Comp>{ std::views::all(std::forward<R2>(r2)), comp };
        }
        template<std::ranges::forward_range R2>
        auto operator()(R2&& r2) const
        {
            return binder<Op, std::views::all_t<R2>, std::ranges::less>{
                std::views::all(std::forward<R2>(r2)), {} };
        }
        template<std::ranges::forward_range R1, std::ranges::forward_range R2>
        auto operator()(R1&& r1, R2&& r2) const
        {
            return (*this)(std::forward<R1>(r1), std::forward<R2>(r2), std::ranges::less{});
        }
    };

} // namespace detail

namespace views {
    /// @brief Lazy overlap of two sorted ranges.
    inline constexpr detail::view_adaptor<set_op::overlap> overlap{};
    /// @brief Lazy subtraction of two sorted ranges.
    inline constexpr detail::view_adaptor<set_op::subtract> subtract{};
    /// @brief Lazy merge of two sorted ranges.
    inline constexpr detail::view_adaptor<set_op::merge> merge{};
    /// @brief Lazy exclusive difference of two sorted ranges.
    inline constexpr detail::view_adaptor<set_op::exclusive> exclusive{};
} // namespace views

