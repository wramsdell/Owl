# WebSocket and Data Framing Specification for Owl

This document outlines the WebSocket protocol and binary data formats used by the Owl device for real-time data streaming.

## 1. Overview

The Owl device uses WebSocket connections for real-time streaming of digital capture data, decoded annotations, and device state updates. All communication happens over a single WebSocket connection established with the device's HTTP server on Core 1.

## 2. WebSocket Connection Details

### 2.1 Connection Endpoint
- **URL**: `ws://<device-ip>/ws`
- **Protocol**: `owl-v1`
- **Connection Type**: Persistent, full-duplex
- **Security**: Unencrypted (device operates in air-gapped environment)
- **Timeout**: 30 seconds of inactivity before connection closure

### 2.2 Connection Authentication
- No authentication required for data streaming
- Configuration changes via REST API require device-specific authentication (not part of WebSocket spec)

## 3. Message Types

WebSocket messages are sent as binary frames with a defined header structure. All multi-byte values are transmitted in little-endian format unless otherwise specified.

### 3.1 Message Header Structure (16 bytes)
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 2    | MessageType    | Identifier for message type (see below)
2      | 2    | Timestamp      | PTP timestamp of this message in nanoseconds
4      | 4    | DataLength     | Length of payload data in bytes
8      | 4    | Reserved       | Reserved for future use (set to 0)
```

### 3.2 Message Types

| Type ID | Name                 | Description |
|---------|----------------------|-------------|
| 0x0001 | CaptureData         | Digital channel capture data |
| 0x0002 | Annotation          | Protocol decoder annotations |
| 0x0003 | DeviceState         | Current device configuration and status |
| 0x0004 | DecoderUpdate       | User decoder metadata or results |
| 0x0005 | TriggerEvent        | Capture trigger notification |

## 4. Data Payload Formats

### 4.1 CaptureData Message (Type 0x0001)

#### Header
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | SampleCount    | Number of samples in this packet
4      | 4    | ChannelMask    | Bitmask indicating active channels (bit 0 = channel 0, etc.)
8      | 4    | SampleRate     | Actual sample rate used for capture (samples per second)
12     | 4    | Reserved       | Reserved for future use
```

#### Payload Data
- Follows the header with raw digital channel data
- Each channel's samples are packed as a sequence of bits
- For 8 channels, each sample is represented by 1 byte (bit 0 = channel 0, bit 1 = channel 1, etc.)
- Bit order within each byte: LSB first (bit 0 = oldest sample in the sequence)

### 4.2 Annotation Message (Type 0x0002)

#### Header
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | AnnotationCount| Number of annotations in this packet
4      | 4    | DecoderID      | ID of decoder that generated this annotation
8      | 4    | Reserved       | Reserved for future use
```

#### Payload Data (Repeated for each annotation)
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | SampleIndex    | Index of sample where annotation starts
4      | 4    | AnnotationType | Type identifier for the annotation
8      | N    | TextLength     | Length of annotation text (variable)
8+N    | ?    | TextData       | UTF-8 encoded text data
```

### 4.3 DeviceState Message (Type 0x0003)

#### Header
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | StateFlags     | Bitmask of current device state flags
4      | 4    | SampleRate     | Currently configured sample rate (samples per second)
8      | 4    | CaptureDepth   | Total capture depth in samples
12     | 4    | TriggerMode    | Current trigger mode identifier
```

#### Payload Data
- Variable length depending on state flags
- May include decoder configurations, channel names, etc.

### 4.4 DecoderUpdate Message (Type 0x0004)

#### Header
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | DecoderID      | Identifier of the decoder being updated
4      | 4    | UpdateType     | Type of update (new, modified, deleted)
8      | 4    | PayloadLength  | Length of payload data in bytes
12     | 4    | Reserved       | Reserved for future use
```

### 4.5 TriggerEvent Message (Type 0x0005)

#### Header
```
Offset | Size | Field Name     | Description
-------|------|----------------|---------------------
0      | 4    | TriggerIndex   | Sample index where trigger occurred
4      | 4    | TriggerType    | Type of trigger that occurred
8      | 4    | Reserved       | Reserved for future use
12     | 4    | Reserved       | Reserved for future use
```

## 5. PTP Timestamp Integration

All messages include a PTP timestamp in nanoseconds since the Unix epoch, providing precise synchronization across multiple Owl devices.

## 6. Performance Considerations

- Maximum sample rate of 200 MSPS with 8 digital channels requires efficient data packing
- Messages are batched to reduce overhead and improve throughput
- Timestamps are captured at packet boundaries for accurate time correlation
