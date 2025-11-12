#pragma once

#include <string>

#include <ada.h>
#include <ada/url_aggregator.h>

bool isValidForWebshotUrl(const ada::url_aggregator &url);
std::string urlToLink(ada::url_aggregator url);
