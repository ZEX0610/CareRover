#pragma once

#include <esp_http_server.h>

// Audio is opt-in at build time; the existing production build is unchanged.
#if defined(CAREROVER_AUDIO_GATEWAY)
bool audioGatewayBegin(httpd_handle_t server);
esp_err_t audioGatewayHandle(httpd_req_t* req);
void audioGatewayClosed(int fd);
bool audioGatewayCallActive();
#else
inline bool audioGatewayCallActive() { return false; }
#endif
