#pragma once

#include "webshot.hpp"
#include <boost/uuid/uuid.hpp>
#include <string>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace us = userver;

namespace v1 {
class [[nodiscard]] WebshotCrud : public us::components::ComponentBase {
public:
    static constexpr std::string_view kName = "webshot-crud";
    explicit WebshotCrud(
        const us::components::ComponentConfig &config,
        const us::components::ComponentContext &context
    );

    ~WebshotCrud();

    void createWebshot(std::string url);
    std::optional<Webshot> findWebshot(boost::uuids::uuid uuid);
    static us::yaml_config::Schema GetStaticConfigSchema();

private:
    std::string webshotRoot;
    ssize_t webshotsPageMax;
    std::string webshotStorageUrl;
    class Impl;
    std::unique_ptr<Impl> impl;
};
}; // namespace v1
