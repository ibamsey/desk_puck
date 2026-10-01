#ifndef DIARY_GCAL_AUTH_H
#define DIARY_GCAL_AUTH_H

#include <Arduino.h>

enum class GcalLinkState {
    NotConfigured,
    PromptConnect,
    DeviceCodePending,
    ShowQr,
    PollingToken,
    Linked,
    LinkFailed,
    NeedsReauth,
};

void gcal_auth_begin();

/** Call every loop (continues poll when user leaves Diary). */
void gcal_auth_poll();

GcalLinkState gcal_auth_link_state();

bool gcal_auth_is_linked();

/** User tapped Connect — requires WiFi. */
void gcal_auth_request_connect();

void gcal_auth_cancel_connect();

/** Clear tokens and return to PromptConnect. */
void gcal_auth_clear_link();

/** QR URL buffer valid in ShowQr / PollingToken. */
const char* gcal_auth_qr_url();

const char* gcal_auth_user_code();

const char* gcal_auth_status_message();

/**
 * Returns access token or nullptr. Starts refresh if needed (async — call poll).
 * On invalid_grant sets NeedsReauth.
 */
const char* gcal_auth_access_token();

#endif // DIARY_GCAL_AUTH_H
