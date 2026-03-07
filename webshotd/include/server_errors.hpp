#pragma once
#include <stdexcept>

namespace v1::errors {

/**
 * @brief Pagination token could not be decoded or mismatched the request.
 */
struct InvalidPageTokenException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief Generic failure while running the crawler.
 */
struct CrawlerFailedException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief Browsertrix sizeLimit exceeded for this crawl.
 */
struct CrawlerSizeLimitException : public CrawlerFailedException {
    using CrawlerFailedException::CrawlerFailedException;
};

} // namespace v1::errors
