#pragma once

/// @file
/// @brief Error codes, and the Result type returned by operations that can reject their input.
///
/// Projectile reports invalid configuration explicitly: functions that take a configuration
/// return a `Result<T>` (the value, or the Error that prevented it) or a
/// `std::optional<Error>`. Expected outcomes, such as a query that hits nothing, are never
/// errors.

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace projectile {

/// Why an operation rejected its input.
enum class Error : std::uint8_t {
    invalid_mass,      ///< A mass is not finite, or not greater than zero.
    invalid_radius,    ///< A radius is negative or not finite.
    invalid_position,  ///< A position is not finite.
    invalid_velocity,  ///< A velocity is not finite.
    invalid_gravity,   ///< A gravity vector is not finite.
    invalid_lifetime,  ///< A lifetime is not greater than zero, or a remaining lifetime is negative
                       ///< or NaN.
    invalid_time_step, ///< A time step is not finite, or not greater than zero.
    invalid_sample_interval, ///< A sample interval is not finite, or not greater than zero.
    invalid_duration,        ///< A duration is negative or not finite.
};

/// A short description of @p error in English, for logs and messages.
[[nodiscard]] constexpr std::string_view describe(Error error) noexcept {
    switch (error) {
    case Error::invalid_mass:
        return "mass must be finite and greater than zero";
    case Error::invalid_radius:
        return "radius must be finite and zero or greater";
    case Error::invalid_position:
        return "position must be finite";
    case Error::invalid_velocity:
        return "velocity must be finite";
    case Error::invalid_gravity:
        return "gravity must be finite";
    case Error::invalid_lifetime:
        return "lifetime must be greater than zero (remaining lifetime zero or greater)";
    case Error::invalid_time_step:
        return "time step must be finite and greater than zero";
    case Error::invalid_sample_interval:
        return "sample interval must be finite and greater than zero";
    case Error::invalid_duration:
        return "duration must be finite and zero or greater";
    }
    return "unknown error";
}

/// Either a value of type @p T or the Error that prevented producing it.
///
/// A Result converts to `true` when it holds a value. Read the value with `*result`,
/// `result->member` or `result.value()`, and the error with `result.error()`. Unlike
/// `std::optional`, every accessor is checked: reading the wrong alternative throws
/// `std::bad_variant_access`.
template <typename T>
class Result {
public:
    /// A result holding @p value.
    constexpr Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : storage_(std::in_place_index<0>, std::move(value)) {}

    /// A result holding @p error.
    constexpr Result(Error error) noexcept : storage_(std::in_place_index<1>, error) {}

    /// Whether the result holds a value.
    [[nodiscard]] constexpr bool has_value() const noexcept { return storage_.index() == 0; }

    /// Whether the result holds a value.
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /// The value. Throws `std::bad_variant_access` if the result holds an error.
    [[nodiscard]] constexpr T& value() & { return std::get<0>(storage_); }
    [[nodiscard]] constexpr const T& value() const& { return std::get<0>(storage_); }
    [[nodiscard]] constexpr T&& value() && { return std::get<0>(std::move(storage_)); }

    /// The value, like value().
    [[nodiscard]] constexpr T& operator*() & { return value(); }
    [[nodiscard]] constexpr const T& operator*() const& { return value(); }
    [[nodiscard]] constexpr T&& operator*() && { return std::move(*this).value(); }
    [[nodiscard]] constexpr T* operator->() { return &value(); }
    [[nodiscard]] constexpr const T* operator->() const { return &value(); }

    /// The error. Throws `std::bad_variant_access` if the result holds a value.
    [[nodiscard]] constexpr Error error() const { return std::get<1>(storage_); }

private:
    std::variant<T, Error> storage_;
};

} // namespace projectile
