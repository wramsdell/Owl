# Owl Data Visualization Approach

## Overview

To visualize the collected data from Owl using HTML/JS, we need to address the key requirements from the specification document:

- Real-time waveform display for digital channels (8 channels)
- Analog channels overlaid on same timeline with independent Y-axis scaling
- Protocol annotation overlays from decoders
- Zoomable, pannable timeline with trigger point indication
- Time cursors with absolute PTP timestamp readout

## Technical Approach

### Core Technologies
1. **WebGL or Canvas API** for high-performance rendering of waveforms at high sample rates
2. **WebSocket** for real-time streaming of capture data from Owl device
3. **Custom waveform renderer** (or optimized D3.js/Chart.js) for performance with large datasets
4. **Pyodide** for running Python decoders in browser (for Post-Capture Python path)

### Architecture Components

#### 1. Data Pipeline
```
Owl Device → WebSocket → Browser (JS) → Visualization Engine → Render
```

- Binary capture data streamed via WebSocket
- Parser to interpret Owl's data format (digital + analog samples)
- Internal data structures for efficient rendering

#### 2. Visualization Engine
- Timeline-based renderer with zoom/pan capabilities
- Multi-channel display with configurable colors and labels
- Protocol annotation overlay system
- Trigger point marker and pre/post-trigger window indication

#### 3. User Interface Components
- Channel management panel (enable/disable, labeling, color selection)
- Zoom/pan controls with timeline scrubber
- Time cursor(s) with absolute PTP timestamp readout
- Annotation display area for decoded protocol information

## Implementation Details

### Rendering Performance Considerations
Based on the specification:
- Maximum sample rate: 200 MSPS (8 channels = 1.6 Gbps)
- At maximum rate, raw data is ~200 MB/s
- With 400 KB SRAM baseline, capture window ~2ms
- For 8 channels at 200 MSPS: ~320,000 samples per channel

**Approach**:
1. **Downsampling**: Implement smart downsampling for display when full resolution isn't needed
2. **Chunked rendering**: Render waveform in chunks to avoid blocking the UI thread
3. **Web Workers**: Offload heavy processing to background threads
4. **Canvas optimization**: Use canvas pixel manipulation for performance

### Timeline System
- Common time axis across all channels
- Zoom/pan functionality with smooth scrolling
- Trigger point clearly marked on timeline
- Pre-trigger and post-trigger window visualization

### Protocol Annotations
- Overlay decoded protocol information on relevant digital channels
- Stacked annotation rows (like Sigrok PulseView)
- Color-coded annotations by decoder type
- Interactive tooltips for detailed information

### Time Synchronization
- PTP timestamps displayed with time cursors
- Inter-device time uncertainty metric in multi-Owl setups
- Support for different time formats (absolute, relative)

## File Structure Proposal

```
owl-visualization/
├── index.html          # Main UI page
├── js/
│   ├── main.js         # Application entry point
│   ├── websocket.js    # WebSocket connection handler
│   ├── renderer.js     # Canvas/WebGL rendering engine
│   ├── data-parser.js  # Parse binary capture data
│   └── ui-controls.js  # UI interaction handlers
├── css/
│   └── styles.css      # Visual styling
├── lib/
│   └── pyodide/        # Pyodide runtime for Python decoders
└── assets/
    └── icons/          # UI icons and graphics
```

## Key Features Implementation

### 1. Waveform Display
- Digital channels as standard logic waveforms
- Analog channels overlaid with independent Y-axis scaling
- Configurable channel colors and labels
- Grid lines and time markers for reference

### 2. Interactivity
- Zoom via mouse wheel or slider
- Pan by dragging timeline
- Hover tooltips for sample values
- Time cursor(s) with absolute PTP timestamps
- Channel selection and configuration

### 3. Protocol Decoding
- Support for Native Decoder annotations (UART, SPI, I2C, etc.)
- User Decoder annotations from Post-Capture Python path
- Color-coded overlays for different protocol types
- Expandable annotation details panel

### 4. Export Capabilities
- Raw capture data export (binary or CSV)
- Decoded annotations as CSV
- Screenshot functionality

## Performance Considerations

### Memory Management
- Efficient binary data handling
- Streaming data processing (don't load entire dataset into memory)
- Garbage collection optimization for long-running sessions

### Rendering Optimization
- Only render visible portions of timeline
- Adaptive sampling rate based on zoom level
- Hardware acceleration via WebGL where possible
- Efficient canvas redraws using requestAnimationFrame

## Browser Compatibility

- Modern browser support (Chrome, Firefox, Safari)
- WebAssembly support for Pyodide
- WebSocket API support
- Canvas API compatibility
- Progressive enhancement approach for older browsers

## Integration Points

### With Existing Owl Components
1. **WebSocket API** - Connect to Owl's REST/WebSocket endpoints
2. **REST API** - Retrieve configuration and decoder information
3. **Native Decoders** - Display built-in protocol decoding results
4. **User Decoders** - Support for Post-Capture Python decoders via Pyodide

### External Libraries
- **D3.js** or custom renderer for timeline visualization
- **Pyodide** for browser-side Python execution (for User Decoder path)
- **Bootstrap** or similar for responsive UI components
