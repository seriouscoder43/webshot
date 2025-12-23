Requirements
* C++20 to build source. This is forced by ada and userver
* Database timezone is UTC

Fixing build
* If some symbols related to Abseil or RE2 are missing when linking, it may help rebuilding userver with USERVER_USE_STATIC_LIBS=OFF.
