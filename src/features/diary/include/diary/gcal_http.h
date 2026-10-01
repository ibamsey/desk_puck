#ifndef DIARY_GCAL_HTTP_H
#define DIARY_GCAL_HTTP_H

#include <WString.h>

namespace gcal_http {

bool post_form(const char* url, const char* body, String& response_out, int& http_code_out);
bool get_bearer(const char* url, const char* bearer, String& response_out, int& http_code_out);

} // namespace gcal_http

#endif // DIARY_GCAL_HTTP_H
