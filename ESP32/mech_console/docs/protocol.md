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
| 12 | sequence | 4 | Caller-provided; no continuity check |
| 16 | payload_length | 2 | 0..512 |
| 18 | reserved | 2 | Zero |

CRC-32/ISO-HDLC covers the decoded header and payload, excluding the CRC itself.
It is stored little-endian. The check value for ASCII `123456789` is `0xCBF43926`.
Packet encode includes the delimiter; packet decode excludes it.

## Messages

| Type | Message | Payload |
| --- | --- | --- |
| 0x01 | GET_STATUS | Empty |
| 0x81 | STATUS_RESPONSE | 16 bytes, below |

The response copies the request's session_id and sequence.

| Response payload offset | Field | Size |
| --- | --- | --- |
| 0 | Uptime in milliseconds | 8 |
| 8 | Current free heap in bytes | 4 |
| 12 | Minimum-ever free heap in bytes | 4 |

## Receive and failure behavior

- Reads may split or combine frames. Empty delimiters are ignored.
- An oversized encoded frame is discarded through the next delimiter.
- Invalid COBS, header, length or CRC causes the packet to be dropped.
- Valid packets are copied into a four-entry FIFO command queue. A full queue rejects the submission and logs a warning; there is no wire BUSY response.
- Unsupported types and nonempty GET_STATUS requests produce no response.
- A successful send means the driver accepted the complete frame, not that the peer received it.

## Pending protocol alignment

The original design reserves `0x01` for HELLO and puts GET_STATUS inside
COMMAND (`0x0A`, opcode `0x0001`), with RESPONSE (`0x0B`). The current assignments
conflict with that design. Change firmware, Python peers and test vectors together.

Handshake, credit, DATA sequencing, heartbeat, request deduplication, END
reconciliation and maintenance barriers are not implemented yet. Header fields
alone do not provide those guarantees.
