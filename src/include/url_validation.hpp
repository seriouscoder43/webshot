#pragma once

#include <ada.h>
#include <ada/url_aggregator.h>
#include <string>

bool isValidForWebshotUrl(const ada::url_aggregator &url);
std::string normalizeUrl(ada::url_aggregator url);
