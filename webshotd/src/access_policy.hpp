#pragma once

#include "expected.hpp"
#include "text.hpp"

#include <memory>

#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace ws {

namespace us = userver;
enum class AccessPolicyError {
    kDbError,
};

enum class AccessPolicyMode {
    kRegular,
    kAllowlistOnly,
};

enum class AccessDecisionReason {
    kAllowed,
    kDenylisted,
    kNotAllowlisted,
    kNonHttps,
};

struct [[nodiscard]] AccessDecision final {
    bool allowed;
    AccessDecisionReason reason;
};

[[nodiscard]] String AccessDecisionMessage(AccessDecisionReason reason);

/**
 * @brief Link prefix access policy management and purge helper.
 *
 * Provides link prefix checks used by the ingestion path, allowlist/denylist
 * administration, and an administrative purge that deletes all captures for a
 * link prefix and nested prefixes.
 */
class [[nodiscard]] AccessPolicyStore : public us::components::ComponentBase {
public:
    static constexpr std::string_view kName = "access_policy";

    explicit AccessPolicyStore(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    ~AccessPolicyStore() override;

    /** @brief Returns true if the normalized prefix key is not deny-listed. */
    [[nodiscard]] Expected<bool, AccessPolicyError> IsAllowedPrefix(const String &prefix_key);
    /** @brief Returns true if the normalized prefix key is deny-listed. */
    [[nodiscard]] Expected<bool, AccessPolicyError> IsDeniedPrefix(const String &prefix_key);
    /** @brief Returns true if the normalized prefix key is allow-listed. */
    [[nodiscard]] Expected<bool, AccessPolicyError> IsAllowlistedPrefix(const String &prefix_key);
    /** @brief Evaluate the normalized prefix key against the configured access policy. */
    [[nodiscard]] Expected<AccessDecision, AccessPolicyError>
    EvaluatePrefix(const String &prefix_key, AccessPolicyMode mode);
    /** @brief Insert a prefix key into the denylist (noop if already present). */
    [[nodiscard]] Expected<void, AccessPolicyError>
    InsertPrefix(const String &prefix_key, const String &reason);
    /** @brief Insert a prefix key into the allowlist (noop if already present). */
    [[nodiscard]] Expected<void, AccessPolicyError>
    InsertAllowlistPrefix(const String &prefix_key, const String &reason);
    /** @brief Remove a prefix key from the allowlist (noop if absent). */
    [[nodiscard]] Expected<void, AccessPolicyError> RemoveAllowlistPrefix(const String &prefix_key);
    static us::yaml_config::Schema GetStaticConfigSchema();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ws
