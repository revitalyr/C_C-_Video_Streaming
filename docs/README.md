# Real-time Video Streaming System - Technical Documentation

Interactive HTML documentation with Mermaid diagrams for the C++ video streaming project.

## Files

- `index.html` - Main documentation with 6 interactive tabs
- `mermaid-height-util.js` - Diagram scaling and sizing utility
- `README.md` - This file

## How to Use

### Opening the Documentation

1. Open `index.html` in a web browser:
   ```bash
   # Linux/macOS
   open index.html

   # Windows
   start index.html
   ```

2. Navigate through tabs using the navigation bar at the top

### Features

- **6 Documentation Tabs:**
  - 📊 Overview - System architecture and capabilities
  - 🏗️ Architecture - C++20 module structure
  - 🔄 Data Flow - End-to-end streaming pipeline
  - 🌐 Network - UDP, RTP, RTSP protocols
  - 🎥 Video - Encoding, jitter buffer, decoding
  - 🚀 Build & Deploy - CMake, Docker, CI/CD

- **Interactive Diagrams:**
  - Zoom in/out with `+` and `−` buttons (50% - 200%)
  - Reset zoom with `⟲` button
  - Scrollable containers for large diagrams
  - Sticky zoom controls

- **Responsive Design:**
  - Works on desktop and mobile devices
  - Touch-friendly navigation
  - Adaptive layout

### Diagram Controls

Each diagram has floating controls in the top-right corner:

| Button | Action |
|--------|--------|
| `−` | Zoom out 10% |
| `%` | Current zoom level |
| `+` | Zoom in 10% |
| `⟲` | Reset to 100% |

## Requirements

- Modern web browser (Chrome, Firefox, Edge, Safari)
- Internet connection for Mermaid CDN
- JavaScript enabled

## Technology Stack

- **Mermaid 10.6.1** - Diagram rendering
- **Vanilla JavaScript** - No frameworks
- **CSS3** - Styling and animations
- **HTML5** - Structure

## Browser Compatibility

| Browser | Version | Status |
|---------|---------|--------|
| Chrome | 90+ | ✅ Supported |
| Firefox | 88+ | ✅ Supported |
| Edge | 90+ | ✅ Supported |
| Safari | 14+ | ✅ Supported |

## Project Information

- **Project:** Real-time Video Streaming System
- **Language:** C++23 with C++20 Modules
- **Focus:** Low-latency H.264 streaming over UDP/RTP
- **Repository:** C_C-_Video_Streaming

## Additional Documentation

See also:
- `LOCAL_TESTING.md` - Local development setup
- `REAL_STREAM_TESTING.md` - Production testing guide
- `ADVANCED.md` - Advanced configuration
- `module_dependencies.md` - Module dependency graph
- `architecture.puml` - PlantUML architecture diagram
- `class_diagram.puml` - PlantUML class diagram
