Requirements
* C++20 to build source. This is forced by ada and userver
* Database timezone is UTC

Fixing build
* If some symbols related to Abseil or RE2 are missing when linking, it may help rebuilding userver with USERVER_USE_STATIC_LIBS=OFF.

1. Start services with compose
2. Start dnscrypt-proxy-docker0, might be already running
3. Run dev.sh
