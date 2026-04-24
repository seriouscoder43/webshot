#pragma once

#include "expected.hpp"
#include "userver_namespaces.hpp"

#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <userver/crypto/base64.hpp>
#include <userver/crypto/exception.hpp>

namespace v1::exu::crypto {

namespace detail {

template <typename E, typename G>
concept CryptoErrorMapper =
    std::invocable<G, const us::crypto::CryptoException &> &&
    std::constructible_from<E, std::invoke_result_t<G, const us::crypto::CryptoException &>>;

template <typename E, typename G, typename F>
    requires CryptoErrorMapper<E, G> && std::invocable<F>
[[nodiscard]] Expected<std::string, E> catchCrypto(F &&f, G &&mapError)
{
    try {
        return std::invoke(std::forward<F>(f));
    } catch (const us::crypto::CryptoException &e) {
        return Unex(E(std::invoke(std::forward<G>(mapError), e)));
    }
}

} // namespace detail

template <typename E, typename G>
    requires detail::CryptoErrorMapper<E, G>
[[nodiscard]] Expected<std::string, E> base64Decode(std::string_view encoded, G &&mapError)
{
    return detail::catchCrypto<E>(
        [&]() { return us::crypto::base64::Base64Decode(encoded); }, std::forward<G>(mapError)
    );
}

template <typename E>
    requires std::copy_constructible<E>
[[nodiscard]] Expected<std::string, E> base64Decode(std::string_view encoded, E error)
{
    return base64Decode<E>(
        encoded, [error = std::move(error)](const us::crypto::CryptoException &) { return error; }
    );
}

template <typename E, typename G>
    requires detail::CryptoErrorMapper<E, G>
[[nodiscard]] Expected<std::string, E> base64UrlDecode(std::string_view encoded, G &&mapError)
{
    return detail::catchCrypto<E>(
        [&]() { return us::crypto::base64::Base64UrlDecode(encoded); }, std::forward<G>(mapError)
    );
}

template <typename E>
    requires std::copy_constructible<E>
[[nodiscard]] Expected<std::string, E> base64UrlDecode(std::string_view encoded, E error)
{
    return base64UrlDecode<E>(
        encoded, [error = std::move(error)](const us::crypto::CryptoException &) { return error; }
    );
}

} // namespace v1::exu::crypto
