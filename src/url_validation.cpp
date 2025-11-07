#include <ada.h>
#include <ada/url_aggregator.h>

#include <string_view>

#include "include/common_definitions.hpp"
#include "include/url_validation.hpp"

bool isValidForWebshotUrl(const ada::url_aggregator &url)
{
    if (url.type != ada::scheme::type::HTTP && url.type != ada::scheme::type::HTTPS)
        return false;
    if (!url.has_hostname() || url.get_hostname().empty())
        return false;
    if (!url.has_valid_domain())
        return false;
    if (url.get_search().size() > v1::kQueryPartLengthMax)
        return false;
    return true;
}

std::string normalizeUrl(ada::url_aggregator url)
{
    url.set_username("");
    url.set_password("");
    url.clear_hash();
    if (auto hostname = url.get_hostname(); hostname.back() == '.')
        url.set_hostname(std::string_view(begin(hostname), end(hostname) - 1));
    return std::string(url.get_href());
}
