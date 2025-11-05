#include "include/url_validation.hpp"
#include "include/common_definitions.hpp"

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
