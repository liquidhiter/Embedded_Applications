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
supported. `0x01` remains reserved for the future HELLO; it does not work yet.
Update firmware and Python peers together.

Handshake, credit, DATA sequencing, heartbeat, request deduplication, END
reconciliation and maintenance barriers are not implemented yet. Header fields
alone do not provide those guarantees.
