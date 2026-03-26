# Module Dependencies Diagram

## Module Structure Overview

```
video_streaming (root namespace)
├── common
│   ├── types (basic types: String, Bytes, etc.)
│   ├── logger (LoggerManager, Logger)
│   ├── interfaces (IReceiver, etc.)
│   └── time (Timestamp, Milliseconds)
├── network
│   ├── endpoint (Endpoint struct)
│   ├── udp_socket (UdpSocket class)
│   ├── sender (Sender class)
│   └── receiver (Receiver class)
├── transport
│   └── rtp
│       ├── packet (RtpPacket class)
│       ├── h264_packetizer (H264Packetizer class)
│       ├── h264_depacketizer (H264Depacketizer class)
│       ├── rtcp (RtcpPacket class)
│       └── rtsp_client (RtspClient class)
├── video
│   ├── media
│   │   ├── frame (Frame, FrameFactory)
│   │   ├── synthetic_encoder (SyntheticH264Encoder)
│   │   ├── ffmpeg_h264_encoder (FFmpegH264Encoder)
│   │   └── rtsp_server (RtspServer)
│   └── jitter
│       └── jitter_buffer (JitterBuffer class)
├── session
│   ├── sender
│   │   └── video_sender (VideoSender class)
│   └── receiver
│       └── video_receiver (VideoReceiver class)
├── async
│   ├── coroutine_types (coroutine utilities)
│   ├── coroutine_frame_generator (frame generation coroutines)
│   ├── coroutine_network_sender (async network sending)
│   └── coroutine_receiver (async receiving)
└── pipeline
    └── pipeline (Pipeline class - main orchestration)
```

## Dependency Graph

```mermaid
graph TD
    %% Core Types
    types[video_streaming.common.types]
    logger[video_streaming.common.logger]
    interfaces[video_streaming.common.interfaces]
    time[video_streaming.common.time]

    %% Network Layer
    endpoint[video_streaming.network.endpoint]
    udp_socket[video_streaming.network.udp_socket]
    sender[video_streaming.network.sender]
    receiver[video_streaming.network.receiver]

    %% RTP Transport
    rtp_packet[video_streaming.rtp.packet]
    h264_packetizer[video_streaming.rtp.h264_packetizer]
    h264_depacketizer[video_streaming.rtp.h264_depacketizer]
    rtcp[video_streaming.rtp.rtcp]
    rtsp_client[video_streaming.rtp.rtsp_client]

    %% Video Processing
    frame[video_streaming.media.frame]
    synthetic_encoder[video_streaming.media.synthetic_encoder]
    ffmpeg_encoder[video_streaming.media.ffmpeg_h264_encoder]
    rtsp_server[video_streaming.media.rtsp_server]
    jitter_buffer[video_streaming.jitter]

    %% Session Layer
    video_sender[video_streaming.session.sender.video_sender]
    video_receiver[video_streaming.session.receiver.video_receiver]

    %% Async Layer
    coroutine_types[video_streaming.async.coroutine_types]
    coroutine_frame_gen[video_streaming.async.coroutine_frame_generator]
    coroutine_sender[video_streaming.async.coroutine_network_sender]
    coroutine_receiver[video_streaming.async.coroutine_receiver]

    %% Pipeline
    pipeline[video_streaming.pipeline]

    %% Dependencies
    endpoint --> types
    udp_socket --> types
    udp_socket --> endpoint
    
    sender --> types
    sender --> udp_socket
    sender --> rtp_packet
    
    receiver --> types
    receiver --> udp_socket
    receiver --> endpoint
    receiver --> rtp_packet
    
    rtp_packet --> types
    h264_packetizer --> types
    h264_packetizer --> rtp_packet
    h264_depacketizer --> types
    h264_depacketizer --> rtp_packet
    h264_depacketizer --> frame
    
    rtcp --> types
    rtsp_client --> types
    rtsp_client --> endpoint
    rtsp_client --> udp_socket
    
    frame --> types
    frame --> time
    synthetic_encoder --> types
    synthetic_encoder --> frame
    ffmpeg_encoder --> types
    ffmpeg_encoder --> frame
    rtsp_server --> types
    rtsp_server --> logger
    rtsp_server --> endpoint
    rtsp_server --> udp_socket
    
    jitter_buffer --> types
    jitter_buffer --> time
    jitter_buffer --> rtp_packet
    
    video_sender --> types
    video_sender --> logger
    video_sender --> frame
    video_sender --> synthetic_encoder
    video_sender --> ffmpeg_encoder
    video_sender --> h264_packetizer
    video_sender --> sender
    
    video_receiver --> types
    video_receiver --> logger
    video_receiver --> interfaces
    video_receiver --> frame
    video_receiver --> h264_depacketizer
    video_receiver --> jitter_buffer
    video_receiver --> receiver
    
    coroutine_types --> types
    coroutine_frame_gen --> types
    coroutine_frame_gen --> frame
    coroutine_frame_gen --> synthetic_encoder
    coroutine_sender --> types
    coroutine_sender --> h264_packetizer
    coroutine_sender --> sender
    coroutine_receiver --> types
    coroutine_receiver --> receiver
    
    pipeline --> logger
    pipeline --> interfaces
    pipeline --> frame
    pipeline --> h264_packetizer
    pipeline --> rtp_packet
    pipeline --> h264_depacketizer
    pipeline --> jitter_buffer
    pipeline --> synthetic_encoder
    pipeline --> types
    pipeline --> receiver
    pipeline --> sender
```

## Class Relationship Diagram

```mermaid
classDiagram
    %% Core Classes
    class LoggerManager {
        +instance() LoggerManager&
        +create_logger(name) std::shared_ptr~Logger~
        +get_logger(name) std::shared_ptr~Logger~
    }
    
    class Logger {
        +info(message)
        +error(message)
        +warning(message)
        +debug(message)
    }
    
    %% Network Classes
    class UdpSocket {
        +is_open() bool
        +open() bool
        +close() void
        +bind(port) bool
        +send(data, endpoint) bool
        +receive_from(buffer, size, sender) int
        -m_socket SocketHandle
        -m_initialized bool
    }
    
    class Sender {
        +start() bool
        +stop() void
        +send(packet) bool
        -m_socket std::unique_ptr~UdpSocket~
        -m_destination Endpoint
    }
    
    class Receiver {
        +start() bool
        +stop() void
        +receive() std::optional~RtpPacket~
        -m_socket std::unique_ptr~UdpSocket~
        -m_port Port
    }
    
    %% RTP Classes
    class RtpPacket {
        +serialize() Bytes
        +deserialize(span) bool
        +get_sequence() uint16_t
        +get_timestamp() uint32_t
        -m_header RtpHeader
        -m_payload Bytes
    }
    
    class H264Packetizer {
        +packetize_frame(data, timestamp) std::vector~RtpPacket~
        -m_ssrc uint32_t
        -m_sequence uint16_t
    }
    
    class H264Depacketizer {
        +process_packet(packet) std::vector~Frame~
        +is_complete_frame() bool
    }
    
    %% Video Classes
    class Frame {
        +data() Bytes
        +timestamp() Timestamp
        +width() int
        +height() int
    }
    
    class SyntheticH264Encoder {
        +encode(frame) std::vector~EncodedFrame~
        -m_width int
        -m_height int
        -m_fps int
        -m_bitrate int
    }
    
    class JitterBuffer {
        +push(packet) void
        +pop(packet) bool
        +size() size_t
        -m_packets std::queue~RtpPacket~
        -m_mutex std::mutex
    }
    
    %% Pipeline Classes
    class Pipeline {
        +start() bool
        +stop() void
        +get_metrics() PipelineMetrics
        -m_encoder SyntheticH264Encoder
        -m_packetizer H264Packetizer
        -m_sender Sender
        -m_receiver Receiver
        -m_jitter_buffer JitterBuffer
        -m_capture_thread std::thread
        -m_process_thread std::thread
    }
    
    %% Relationships
    LoggerManager --> Logger : creates
    Sender --> UdpSocket : uses
    Receiver --> UdpSocket : uses
    H264Packetizer --> RtpPacket : creates
    H264Depacketizer --> RtpPacket : processes
    JitterBuffer --> RtpPacket : stores
    Pipeline --> SyntheticH264Encoder : uses
    Pipeline --> H264Packetizer : uses
    Pipeline --> Sender : uses
    Pipeline --> Receiver : uses
    Pipeline --> JitterBuffer : uses
```

## Key Design Patterns

### 1. Module-Based Architecture
- Each logical component is a separate C++20 module
- Clear separation between interface (.ixx) and implementation (.cpp)
- Explicit import dependencies

### 2. Network Layer Abstraction
- `UdpSocket` provides low-level UDP operations
- `Sender` and `Receiver` provide high-level networking
- Clean separation between transport and application logic

### 3. RTP Protocol Implementation
- `RtpPacket` handles serialization/deserialization
- `H264Packetizer` converts H.264 frames to RTP packets
- `H264Depacketizer` converts RTP packets back to frames

### 4. Video Processing Pipeline
- `Frame` represents video data
- `SyntheticH264Encoder` generates test H.264 streams
- `JitterBuffer` handles network packet reordering and loss

### 5. Orchestration
- `Pipeline` coordinates all components
- Multi-threaded capture and processing loops
- Clean lifecycle management (start/stop)

## Module Import Hierarchy

```
Level 1 (Foundation):
├── video_streaming.common.types
├── video_streaming.common.time
└── video_streaming.common.logger

Level 2 (Basic Services):
├── video_streaming.common.interfaces
├── video_streaming.network.endpoint
└── video_streaming.network.udp_socket

Level 3 (Transport):
├── video_streaming.network.sender
├── video_streaming.network.receiver
├── video_streaming.rtp.packet
└── video_streaming.media.frame

Level 4 (Protocol):
├── video_streaming.rtp.h264_packetizer
├── video_streaming.rtp.h264_depacketizer
├── video_streaming.media.synthetic_encoder
└── video_streaming.jitter

Level 5 (Application):
├── video_streaming.session.sender.video_sender
├── video_streaming.session.receiver.video_receiver
└── video_streaming.async.*

Level 6 (Orchestration):
└── video_streaming.pipeline
```

This modular design ensures:
- **Clear dependencies**: Each module only imports what it needs
- **Fast compilation**: Modules can be compiled independently
- **Maintainability**: Changes are isolated to specific modules
- **Testability**: Individual modules can be unit tested
- **Reusability**: Modules can be reused in different contexts
