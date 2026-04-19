#pragma once

#include "text.hpp"
#include "userver_namespaces.hpp"

#include <userver/utils/strong_typedef.hpp>

namespace v1::s3v4 {

using AccessKeyId = us::utils::NonLoggable<class AccessKeyIdTag, String>;
using SecretAccessKey = us::utils::NonLoggable<class SecretAccessKeyTag, String>;
using SessionToken = us::utils::NonLoggable<class SessionTokenTag, String>;

} // namespace v1::s3v4
