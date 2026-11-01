# Current UART protocol

This describes the implemented protocol, not all messages in the original design.
It is an experimental interface and will change when session management is added.

## Framing

```text
COBS(Header || Payload || CRC32) || 00

Decoded packet:
+------------------+------------------+------------------+
| Header: 20 bytes | Payload: 0..512  | CRC32: 4 bytes   |
+------------------+------------------+------------------+
```

Multibyte integers are little-endian. Maximum encoded body: 539 bytes;
maximum wire frame including delimiter: 540 bytes.

| Header offset | Field | Size | Current rule |
| --- | --- | --- | --- |
| 0 | version | 1 | 1 |
| 1 | type | 1 | See messages below |
| 2 | flags | 2 | Zero |
| 4 | session_id | 8 | Caller-provided; no active-session validation |
| 12 | sequence | 4 | Zero for COMMAND/RESPONSE; DATA sequencing pending |
| 16 | payload_length | 2 | 0..512 |
| 18 | reserved | 2 | Zero |

CRC-32/ISO-HDLC covers the decoded header and payload, excluding the CRC itself.
It is stored little-endian. The check value for ASCII `123456789` is `0xCBF43926`.
Packet encode includes the delimiter; packet decode excludes it.

## Messages

| Type | Message | Payload |
| --- | --- | --- |
| 0x0A | COMMAND | request_id u32, opcode u16, args_length u16, args |
| 0x0B | RESPONSE | request_id u32, status u16, result_length u16, result |

Arguments/results are at most 56 bytes; the envelope is 8 bytes. The declared
argument length must exactly match the payload. Responses echo session_id and
request_id, with header sequence zero. Request IDs are currently caller-provided;
ordering and deduplication are not enforced yet.

GET_STATUS is opcode `0x0001`, takes no arguments and returns status OK with:

| Result offset | Field | Size |
| --- | --- | --- |
| 0 | Uptime in milliseconds | 8 |
| 8 | Current free heap in bytes | 4 |
| 12 | Minimum-ever free heap in bytes | 4 |

Wire statuses currently implemented: OK=0, INVALID=1, UNSUPPORTED=4.
GET_STATUS with arguments returns INVALID; unknown opcodes return UNSUPPORTED.
Error responses contain no result bytes. A status code is separate from the
local ESP-IDF return value, which reports response submission success/failure.

## HELLO handshake (in development)

HELLO validation, pending-session creation and repeatable ACK construction are
implemented and component-tested. The full handshake is still in development
and is not yet wired into the running command pipeline.
API ownership and error behavior are documented in
[`app_session.h`](https://github.com/liquidhiter/Embedded_Applications/blob/main/ESP32/mech_console/components/app_session/include/app_session.h).

```text
Host                                      Device
HELLO (type 0x01, session 0) ------------> Validate and generate session ID
                            <------------ HELLO_ACK (type 0x02, new session)
                                          State: DOWN -> HANDSHAKE
```

HELLO_ACK is a dedicated packet type, not a COMMAND RESPONSE. It has no
request_id/status/result_length envelope. All integers below are little-endian;
payload offsets start at zero after the decoded 20-byte packet header.

### HELLO request

Header: type `0x01`, session_id `0`, sequence `0`, payload_length `16`.

| Payload offset | Field | Size | Rule |
| --- | --- | --- | --- |
| 0..7 | host_nonce | u64 | Host-generated identifier for this handshake attempt |
| 8..9 | requested_window | u16 | 1..8 DATA frames |
| 10..11 | max_payload | u16 | 1..512 bytes |
| 12..15 | features | u32 | Zero in the baseline |

### HELLO_ACK response

Header: type `0x02`, device-generated nonzero session_id, sequence `0`,
payload_length `24`. This first implementation accepts the requested window and
maximum payload within the supported ranges.

| Payload offset | Field | Size | Meaning |
| --- | --- | --- | --- |
| 0..7 | host_nonce | u64 | Copy of the accepted HELLO nonce |
| 8..15 | device_boot_id | u64 | Nonzero identifier generated once per device boot |
| 16..17 | accepted_window | u16 | Accepted DATA window in frames |
| 18..19 | max_payload | u16 | Negotiated maximum payload in bytes |
| 20..23 | features | u32 | Zero in the baseline |

Example decoded response for nonce `0x1122334455667788`, boot ID
`0x0102030405060708`, window 8 and maximum payload 512:

```text
Header fields:
  version        = 1
  type           = 0x02
  flags          = 0
  session_id     = 0x8877665544332211  (example generated ID)
  sequence       = 0
  payload_length = 24
  reserved       = 0

Payload bytes:
  88 77 66 55 44 33 22 11   host_nonce
  08 07 06 05 04 03 02 01   device_boot_id
  08 00                     accepted_window
  00 02                     max_payload
  00 00 00 00               features
```

`app_session_handle_hello()` fills a `frame_packet_t`; it does not send bytes.
The caller passes the successful response to `app_link_send()`, which adds the
packet header, CRC, COBS encoding and delimiter through the existing encoder.

An identical HELLO received during HANDSHAKE reconstructs the same ACK without
calling the ID generator again. A changed nonce or parameters is rejected.
If sending the ACK fails, keep the pending state so the host can retry.
Errors from the handler leave the instance and response unchanged; send the
response only after `ESP_OK`.

ACK preparation or transmission does not enter ACTIVE or grant DATA credit.
CONFIRM/READY, handshake timeouts and credit are subsequent implementation steps.

## Receive and failure behavior

- Reads may split or combine frames. Empty delimiters are ignored.
- An oversized encoded frame is discarded through the next delimiter.
- Invalid COBS, header, length or CRC causes the packet to be dropped.
- Valid packets are copied into a four-entry FIFO command queue. A full queue rejects the submission and logs a warning; there is no wire BUSY response.
- Unsupported packet types, malformed envelopes and nonzero control sequences produce no response.
- A successful send means the driver accepted the complete frame, not that the peer received it.

## Compatibility and pending work

COMMAND/RESPONSE types and the GET_STATUS opcode now match the original design.
The previous experimental `0x01` GET_STATUS / `0x81` response are no longer
supported. HELLO `0x01` is defined in the session helper but is not yet routed by
the production application.
Update firmware and Python peers together.

Handshake, credit, DATA sequencing, heartbeat, request deduplication, END
reconciliation and maintenance barriers are not implemented yet. Header fields
alone do not provide those guarantees.
