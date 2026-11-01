#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "app_session.h"
#include "app_utils.h"

#define BOOT_ID UINT64_C(0x0102030405060708)
#define SESSION_ID UINT64_C(0x8877665544332211)
#define HOST_NONCE UINT64_C(0x1122334455667788)

typedef struct {
    unsigned calls;
    uint64_t value;
} id_source_t;

/* Keep packet snapshots off the small Unity main-task stack. */
static app_session_t session;
static id_source_t source;
static frame_packet_t request, response;
static uint8_t request_before[sizeof(frame_packet_t)];
static uint8_t response_before[sizeof(frame_packet_t)];
static unsigned null_context_calls;

static uint64_t generate_id(void *context)
{
    if (context == NULL) {
        ++null_context_calls;
        return SESSION_ID;
    }
    id_source_t *ids = context;
    ++ids->calls;
    return ids->value;
}

static void set_hello(void)
{
    request = (frame_packet_t){.type = 0x01, .payload_length = 16};
    /* Independent HELLO bytes: nonce, window 8, payload limit 512, features 0. */
    const uint8_t payload[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x08, 0x00, 0x00, 0x02, 0, 0, 0, 0};
    memcpy(request.payload, payload, sizeof(payload));
}

static void prepare(void)
{
    source = (id_source_t){.value = SESSION_ID};
    set_hello();
    memset(&response, 0xA5, sizeof(response));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_init(&session, BOOT_ID, generate_id, &source));
}

static void expect_error(esp_err_t expected)
{
    uint8_t session_before[sizeof(session)];
    memcpy(session_before, &session, sizeof(session));
    memcpy(request_before, &request, sizeof(request));
    memcpy(response_before, &response, sizeof(response));
    unsigned calls = source.calls;
    TEST_ASSERT_EQUAL(expected, app_session_handle_hello(&session, &request, &response));
    TEST_ASSERT_EQUAL_MEMORY(session_before, &session, sizeof(session));
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_MEMORY(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL_UINT(calls, source.calls);
}

static void set_confirm(void)
{
    request = (frame_packet_t){.type = 0x03, .session_id = SESSION_ID, .payload_length = 8};
    const uint8_t nonce[] = {0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    memcpy(request.payload, nonce, sizeof(nonce));
}

static void prepare_confirm(void)
{
    prepare();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    set_confirm();
    memset(&response, 0xA5, sizeof(response));
}

static void expect_confirm_error(esp_err_t expected)
{
    uint8_t before[sizeof(session)];
    memcpy(before, &session, sizeof(session));
    memcpy(request_before, &request, sizeof(request));
    memcpy(response_before, &response, sizeof(response));
    unsigned calls = source.calls;
    TEST_ASSERT_EQUAL(expected, app_session_handle_confirm(&session, &request, &response));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_MEMORY(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL_UINT(calls, source.calls);
}

static void expect_ready(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x04, response.type);
    TEST_ASSERT_TRUE(response.session_id == SESSION_ID);
    TEST_ASSERT_EQUAL_UINT32(0, response.sequence);
    TEST_ASSERT_EQUAL_UINT16(0, response.payload_length);
    /* Independent golden wire bytes from Python struct/zlib, including CRC/COBS. */
    const uint8_t expected[] = {0x03, 0x01, 0x04, 0x01, 0x09, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x05, 0xAA, 0x5A, 0xF4, 0x94, 0x00};
    uint8_t wire[sizeof(expected)];
    size_t length = 0;
    TEST_ASSERT_EQUAL(FRAME_CODEC_OK, frame_packet_encode(&response, wire, sizeof(wire), &length));
    TEST_ASSERT_EQUAL_UINT(sizeof(expected), length);
    TEST_ASSERT_EQUAL_MEMORY(expected, wire, sizeof(expected));
}

TEST_CASE("Session init validates arguments and preserves failed output", "[app_session]")
{
    memset(&session, 0xA5, sizeof(session));
    uint8_t before[sizeof(session)];
    memcpy(before, &session, sizeof(session));
    source = (id_source_t){.value = SESSION_ID};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_init(NULL, BOOT_ID, generate_id, &source));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_init(&session, 0, generate_id, &source));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_init(&session, BOOT_ID, NULL, &source));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_init(&session, BOOT_ID, generate_id, &source));
    TEST_ASSERT_EQUAL(APP_SESSION_DOWN, session.state);
    TEST_ASSERT_TRUE(session.device_boot_id == BOOT_ID);
    TEST_ASSERT_TRUE(session.session_id == 0 && session.host_nonce == 0);
    TEST_ASSERT_EQUAL_UINT16(0, session.accepted_window);
    TEST_ASSERT_EQUAL_UINT16(0, session.max_payload);
    TEST_ASSERT_TRUE(session.generate_id == generate_id);
    TEST_ASSERT_EQUAL_PTR(&source, session.context);
    TEST_ASSERT_EQUAL_UINT(0, source.calls);
}

TEST_CASE("Session rejects null HELLO arguments without side effects", "[app_session]")
{
    prepare();
    uint8_t before[sizeof(session)];
    memcpy(before, &session, sizeof(session));
    memcpy(response_before, &response, sizeof(response));
    memcpy(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_hello(NULL, &request, &response));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_hello(&session, NULL, &response));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_hello(&session, &request, NULL));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL_MEMORY(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT(0, source.calls);
}

TEST_CASE("Session HELLO produces the exact ACK layout", "[app_session]")
{
    prepare();
    memcpy(request_before, &request, sizeof(request));
    const uint8_t expected[] = {
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x08, 0x07, 0x06, 0x05,
        0x04, 0x03, 0x02, 0x01, 0x08, 0x00, 0x00, 0x02, 0,    0,    0,    0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    TEST_ASSERT_EQUAL_HEX8(0x02, response.type);
    TEST_ASSERT_TRUE(response.session_id == SESSION_ID);
    TEST_ASSERT_EQUAL_UINT32(0, response.sequence);
    TEST_ASSERT_EQUAL_UINT16(24, response.payload_length);
    TEST_ASSERT_EQUAL_MEMORY(expected, response.payload, sizeof(expected));
    TEST_ASSERT_EQUAL(APP_SESSION_HANDSHAKE, session.state);
    TEST_ASSERT_TRUE(session.session_id == SESSION_ID && session.host_nonce == HOST_NONCE);
    TEST_ASSERT_TRUE(session.device_boot_id == BOOT_ID);
    TEST_ASSERT_EQUAL_UINT16(8, session.accepted_window);
    TEST_ASSERT_EQUAL_UINT16(512, session.max_payload);
    TEST_ASSERT_EQUAL_PTR(&source, session.context);
    TEST_ASSERT_TRUE(session.generate_id == generate_id);
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
}

TEST_CASE("Session accepts nonce and negotiation boundaries", "[app_session]")
{
    const uint64_t nonces[] = {0, UINT64_MAX};
    const uint16_t windows[] = {1, 8};
    const uint16_t limits[] = {1, 512};
    for (size_t n = 0; n < 2; ++n) {
        for (size_t w = 0; w < 2; ++w) {
            for (size_t p = 0; p < 2; ++p) {
                prepare();
                app_write_u64_le(request.payload, nonces[n]);
                app_write_u16_le(request.payload + 8, windows[w]);
                app_write_u16_le(request.payload + 10, limits[p]);
                TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
                TEST_ASSERT_TRUE(session.host_nonce == nonces[n]);
                TEST_ASSERT_EQUAL_UINT16(windows[w], session.accepted_window);
                TEST_ASSERT_EQUAL_UINT16(limits[p], session.max_payload);
                TEST_ASSERT_TRUE(app_read_u64_le(response.payload) == nonces[n]);
                TEST_ASSERT_EQUAL_UINT16(windows[w], app_read_u16_le(response.payload + 16));
                TEST_ASSERT_EQUAL_UINT16(limits[p], app_read_u16_le(response.payload + 18));
            }
        }
    }
}

static void check_invalid_requests(void)
{
    const uint8_t types[] = {0, 2, 0x0A, UINT8_MAX};
    for (size_t i = 0; i < sizeof(types); ++i) {
        set_hello();
        request.type = types[i];
        expect_error(ESP_ERR_INVALID_ARG);
    }
    const uint16_t lengths[] = {0, 1, 15, 17, 512, UINT16_MAX};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        set_hello();
        request.payload_length = lengths[i];
        /* Poison bytes outside the declared payload. Length rejection must
         * precede comparing these bytes with pending-handshake parameters.
         */
        memset(request.payload, 0xA5, sizeof(request.payload));
        expect_error(ESP_ERR_INVALID_ARG);
    }
    set_hello();
    request.session_id = SESSION_ID;
    expect_error(ESP_ERR_INVALID_ARG);
    set_hello();
    request.sequence = UINT32_MAX;
    expect_error(ESP_ERR_INVALID_ARG);
    const uint16_t windows[] = {0, 9, UINT16_MAX};
    for (size_t i = 0; i < sizeof(windows) / sizeof(windows[0]); ++i) {
        set_hello();
        app_write_u16_le(request.payload + 8, windows[i]);
        expect_error(ESP_ERR_INVALID_ARG);
    }
    const uint16_t limits[] = {0, 513, UINT16_MAX};
    for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        set_hello();
        app_write_u16_le(request.payload + 10, limits[i]);
        expect_error(ESP_ERR_INVALID_ARG);
    }
    /* Every unsupported feature bit must be rejected. */
    for (unsigned bit = 0; bit < 32; ++bit) {
        set_hello();
        app_write_u32_le(request.payload + 12, UINT32_C(1) << bit);
        expect_error(ESP_ERR_INVALID_ARG);
    }
}

TEST_CASE("Session rejects malformed HELLO in DOWN and HANDSHAKE", "[app_session]")
{
    prepare();
    check_invalid_requests();
    set_hello();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    check_invalid_requests();
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
}

TEST_CASE("Session repeats ACK without generating another ID", "[app_session]")
{
    prepare();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    const uint64_t original_id = session.session_id;
    uint8_t original_payload[24];
    memcpy(original_payload, response.payload, sizeof(original_payload));
    /* Simulate a generator that would fail if incorrectly called on retry. */
    source.value = 0;
    for (unsigned retry = 0; retry < 4; ++retry) {
        memset(&response, 0xA5, sizeof(response));
        TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
        TEST_ASSERT_TRUE(response.session_id == original_id);
        TEST_ASSERT_EQUAL_HEX8(2, response.type);
        TEST_ASSERT_EQUAL_UINT32(0, response.sequence);
        TEST_ASSERT_EQUAL_UINT16(24, response.payload_length);
        TEST_ASSERT_EQUAL_MEMORY(original_payload, response.payload, sizeof(original_payload));
        TEST_ASSERT_EQUAL_UINT(1, source.calls);
        TEST_ASSERT_EQUAL(APP_SESSION_HANDSHAKE, session.state);
    }
}

TEST_CASE("Session rejects conflicting HELLO and disallowed states", "[app_session]")
{
    prepare();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    app_write_u64_le(request.payload, HOST_NONCE + 1);
    expect_error(ESP_ERR_INVALID_STATE);
    set_hello();
    app_write_u16_le(request.payload + 8, 7);
    expect_error(ESP_ERR_INVALID_STATE);
    set_hello();
    app_write_u16_le(request.payload + 10, 511);
    expect_error(ESP_ERR_INVALID_STATE);
    set_hello();
    session.state = APP_SESSION_ACTIVE;
    expect_error(ESP_ERR_INVALID_STATE);
    session.state = (app_session_state_t)99;
    expect_error(ESP_ERR_INVALID_STATE);
}

TEST_CASE("Session preserves outputs when generation fails then retries", "[app_session]")
{
    prepare();
    source.value = 0;
    uint8_t before[sizeof(session)];
    memcpy(before, &session, sizeof(session));
    memcpy(response_before, &response, sizeof(response));
    memcpy(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL(ESP_FAIL, app_session_handle_hello(&session, &request, &response));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL_MEMORY(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
    source.value = UINT64_MAX;
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    TEST_ASSERT_TRUE(response.session_id == UINT64_MAX);
    TEST_ASSERT_EQUAL_UINT(2, source.calls);
}

TEST_CASE("Session instances keep independent IDs and callback contexts", "[app_session]")
{
    prepare();
    app_session_t other;
    id_source_t other_source = {.value = 42};
    TEST_ASSERT_EQUAL(ESP_OK, app_session_init(&other, 7, generate_id, &other_source));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&other, &request, &response));
    TEST_ASSERT_TRUE(session.session_id == SESSION_ID && other.session_id == 42);
    TEST_ASSERT_TRUE(session.device_boot_id == BOOT_ID && other.device_boot_id == 7);
    TEST_ASSERT_EQUAL_PTR(&source, session.context);
    TEST_ASSERT_EQUAL_PTR(&other_source, other.context);
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
    TEST_ASSERT_EQUAL_UINT(1, other_source.calls);

    /* NULL is a valid borrowed context if the callback supports it. */
    app_session_t no_context;
    null_context_calls = 0;
    TEST_ASSERT_EQUAL(ESP_OK, app_session_init(&no_context, BOOT_ID, generate_id, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&no_context, &request, &response));
    TEST_ASSERT_EQUAL_PTR(NULL, no_context.context);
    TEST_ASSERT_EQUAL_UINT(1, null_context_calls);
}

TEST_CASE("Session CONFIRM rejects null arguments without side effects", "[app_session]")
{
    prepare_confirm();
    uint8_t before[sizeof(session)];
    memcpy(before, &session, sizeof(session));
    memcpy(request_before, &request, sizeof(request));
    memcpy(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_confirm(NULL, &request, &response));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_confirm(&session, NULL, &response));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, app_session_handle_confirm(&session, &request, NULL));
    TEST_ASSERT_EQUAL_MEMORY(before, &session, sizeof(session));
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL_MEMORY(response_before, &response, sizeof(response));
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
}

TEST_CASE("Session CONFIRM activates and returns exact READY wire bytes", "[app_session]")
{
    prepare_confirm();
    app_session_t before = session;
    memcpy(request_before, &request, sizeof(request));
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
    expect_ready();
    TEST_ASSERT_EQUAL(APP_SESSION_ACTIVE, session.state);
    TEST_ASSERT_TRUE(session.session_id == before.session_id);
    TEST_ASSERT_TRUE(session.host_nonce == before.host_nonce);
    TEST_ASSERT_TRUE(session.device_boot_id == before.device_boot_id);
    TEST_ASSERT_EQUAL_UINT16(before.accepted_window, session.accepted_window);
    TEST_ASSERT_EQUAL_UINT16(before.max_payload, session.max_payload);
    TEST_ASSERT_TRUE(session.generate_id == before.generate_id);
    TEST_ASSERT_EQUAL_PTR(before.context, session.context);
    TEST_ASSERT_EQUAL_UINT(1, source.calls);
    TEST_ASSERT_EQUAL_MEMORY(request_before, &request, sizeof(request));
}

TEST_CASE("Session CONFIRM validates headers before state or payload", "[app_session]")
{
    for (unsigned stage = 0; stage < 3; ++stage) {
        if (stage == 0) {
            prepare();
        } else {
            prepare_confirm();
            if (stage == 2) {
                TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
            }
        }
        const uint8_t types[] = {0, 1, 2, 4, 0x0A, UINT8_MAX};
        for (size_t i = 0; i < sizeof(types); ++i) {
            set_confirm();
            request.type = types[i];
            expect_confirm_error(ESP_ERR_INVALID_ARG);
        }
        const uint16_t lengths[] = {0, 1, 7, 9, 512, UINT16_MAX};
        for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
            set_confirm();
            request.payload_length = lengths[i];
            memset(request.payload, 0xA5, sizeof(request.payload));
            expect_confirm_error(ESP_ERR_INVALID_ARG);
        }
        set_confirm();
        request.session_id = 0;
        expect_confirm_error(ESP_ERR_INVALID_ARG);
        set_confirm();
        request.sequence = UINT32_MAX;
        expect_confirm_error(ESP_ERR_INVALID_ARG);
    }
}

TEST_CASE("Session CONFIRM rejects mismatches and disallowed states", "[app_session]")
{
    prepare();
    set_confirm();
    expect_confirm_error(ESP_ERR_INVALID_STATE);
    for (unsigned active = 0; active < 2; ++active) {
        prepare_confirm();
        if (active) {
            TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
        }
        request.session_id = SESSION_ID + 1;
        expect_confirm_error(ESP_ERR_INVALID_STATE);
        set_confirm();
        app_write_u64_le(request.payload, HOST_NONCE + 1);
        expect_confirm_error(ESP_ERR_INVALID_STATE);
        /* A rejected CONFIRM must not invalidate the accepted session. */
        set_confirm();
        TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
        expect_ready();
    }
    set_confirm();
    session.state = (app_session_state_t)99;
    expect_confirm_error(ESP_ERR_INVALID_STATE);
}

TEST_CASE("Session repeats READY after loss without changing metadata", "[app_session]")
{
    prepare_confirm();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
    app_session_t before = session;
    /* Discard READY to simulate loss; a wrong generator call must now fail. */
    source.value = 0;
    for (unsigned retry = 0; retry < 4; ++retry) {
        memset(&response, 0xA5, sizeof(response));
        TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
        expect_ready();
        TEST_ASSERT_EQUAL(APP_SESSION_ACTIVE, session.state);
        TEST_ASSERT_TRUE(session.session_id == before.session_id);
        TEST_ASSERT_TRUE(session.host_nonce == before.host_nonce);
        TEST_ASSERT_TRUE(session.device_boot_id == before.device_boot_id);
        TEST_ASSERT_EQUAL_UINT16(before.accepted_window, session.accepted_window);
        TEST_ASSERT_EQUAL_UINT16(before.max_payload, session.max_payload);
        TEST_ASSERT_TRUE(session.generate_id == before.generate_id);
        TEST_ASSERT_EQUAL_PTR(before.context, session.context);
        TEST_ASSERT_EQUAL_UINT(1, source.calls);
    }
    set_hello();
    expect_error(ESP_ERR_INVALID_STATE);
    set_confirm();
    TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
    expect_ready();
}

TEST_CASE("Session CONFIRM accepts zero nonce and maximum identifiers", "[app_session]")
{
    const uint64_t nonces[] = {0, UINT64_MAX};
    for (size_t i = 0; i < 2; ++i) {
        prepare();
        source.value = UINT64_MAX;
        app_write_u64_le(request.payload, nonces[i]);
        TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_hello(&session, &request, &response));
        set_confirm();
        request.session_id = UINT64_MAX;
        app_write_u64_le(request.payload, nonces[i]);
        TEST_ASSERT_EQUAL(ESP_OK, app_session_handle_confirm(&session, &request, &response));
        TEST_ASSERT_EQUAL(APP_SESSION_ACTIVE, session.state);
        TEST_ASSERT_TRUE(response.session_id == UINT64_MAX);
        TEST_ASSERT_EQUAL_HEX8(4, response.type);
        TEST_ASSERT_EQUAL_UINT16(0, response.payload_length);
        TEST_ASSERT_EQUAL_UINT32(0, response.sequence);
        TEST_ASSERT_EQUAL_UINT(1, source.calls);
    }
}
