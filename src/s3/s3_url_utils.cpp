#include "s3/s3_url_utils.hpp"

#include <string>
#include <string_view>

#include <ada/unicode.h>

namespace v1::s3v4 {

std::vector<std::pair<std::string, std::string>> decodeQueryString(std::string_view search)
{
    std::vector<std::pair<std::string, std::string>> query;
    if (search.empty())
        return query;

    std::string searchCopy(search);
    if (!searchCopy.empty() && searchCopy.front() == '?')
        searchCopy.erase(searchCopy.begin());

    size_t pos = 0;
    while (pos < searchCopy.size()) {
        const auto amp = searchCopy.find('&', pos);
        const auto eq = searchCopy.find('=', pos);
        if (eq == std::string::npos)
            break;
        const std::string keyPart = searchCopy.substr(pos, eq - pos);
        const std::string valPart = searchCopy.substr(
            eq + 1, amp == std::string::npos ? std::string::npos : amp - eq - 1
        );
        const auto keyPercent = keyPart.find('%');
        const auto valPercent = valPart.find('%');
        const std::string key = ada::unicode::percent_decode(
            keyPart, keyPercent == std::string::npos ? std::string::npos : keyPercent
        );
        const std::string value = ada::unicode::percent_decode(
            valPart, valPercent == std::string::npos ? std::string::npos : valPercent
        );
        query.emplace_back(key, value);
        if (amp == std::string::npos)
            break;
        pos = amp + 1;
    }
    return query;
}

} // namespace v1::s3v4
