#pragma once

#include "text.hpp"

#include <userver/utils/strong_typedef.hpp>

namespace v1::s3v4 {

using AccessKeyId = userver::utils::NonLoggable<class AccessKeyIdTag, String>;
using SecretAccessKey = userver::utils::NonLoggable<class SecretAccessKeyTag, String>;
using SessionToken = userver::utils::NonLoggable<class SessionTokenTag, String>;

} // namespace v1::s3v4
