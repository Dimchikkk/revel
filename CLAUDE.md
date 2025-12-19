# Revel

Professional note-taking and brainstorming application with infinite canvas and powerful organization features.

## Features:

### Core Architecture
* **Single-file database:** All data stored in portable SQLite3 database file
* **Full-text search:** Advanced BM25 ranking algorithm for content discovery
* **Infinite canvas:** Zoom, pan, and organize content in unlimited 2D space
* **Nested spaces:** Organize elements into hierarchical spaces with infinite depth
* **Cross-space operations:** Move and search elements across different spaces

### Content Types
* **Paper Notes:** Simple, lightweight text notes for quick thoughts
* **Rich Notes:** Advanced text notes with full formatting capabilities
* **Media Notes:** Container notes that can embed images, videos, and audio
* **Shapes:** Geometric elements (circles, rectangles, triangles, diamonds, cylinders)
    * Configurable stroke width and fill options
    * Text labels with custom fonts
* **Freehand Drawing:** Free-form pen tool with adjustable stroke width
* **Images:** Direct clipboard paste and drag-and-drop support
* **Videos:** MP4 video playback with thumbnail previews
* **Audio:** MP3 audio playback with duration display and album art extraction
    * Automatic playlist chaining via arrow connections
    * Duration extraction and display (MM:SS format)
    * Album art thumbnail support (if embedded in MP3)
    * Lazy loading for efficient memory usage

### Connections & Relationships
* **Smart Arrows:** Connect any elements with visual relationships
    * **Parallel arrows:** Curved connections with automatic routing
    * **Straight arrows:** Direct point-to-point connections
    * **Arrowhead types:** None, single, or double arrowheads
* **Connection points:** 8-point connection system for precise linking

### Element Management
* **Smart placement:** New elements automatically positioned in closest empty space from viewport center
    * Spiral search algorithm finds optimal placement
    * Collision detection prevents overlapping elements
    * Preserves manual placement for drag-and-drop and interactive drawing
* **Flexible editing:** Resize, move, rotate, and style all element types
* **Element descriptions:** Private comments/metadata with creation timestamps
* **Background colors:** Customize appearance of any element
* **Z-order control:** Layer elements with bring-to-front functionality
* **Element cloning:**
    * **Fork:** Create independent copies
    * **Clone by text:** Share text content across elements
    * **Clone by size:** Share dimensions across elements

### Advanced Features
* **Comprehensive undo/redo:** Track all actions with detailed history log
* **Drag & drop:** Import images, videos, and audio files directly from file manager
* **Keyboard shortcuts:** Full productivity shortcuts for all major actions
* **Font management:** Complete typography control with family, size, and style
* **Color management:** Advanced color picker for all visual elements
* **Thumbnail customization:** Change thumbnails for media elements (images, videos, audio)
* **Action logging:** Timestamped history of all user actions

### User Interface
* **Context menus:** Right-click access to all element operations
* **Shape designer:** Visual shape selection and configuration dialog
* **Search interface:** Global content search across all spaces
* **Space navigation:** Easy switching between hierarchical workspaces
* **Space tree view:** Hierarchical tree view of all spaces with navigation
    * Toggle visibility with dedicated toolbar button
    * Real-time updates as spaces are created/modified
    * Shows element count for each space
    * Quick navigation by clicking on any space
* **Drawing toolbar:** Tools for freehand drawing and shape creation

### Automation & Scripting
* **DSL (Domain Specific Language):** Programmatically create complex layouts
    * Batch element creation (notes, shapes, images, videos, audio)
    * Automated connections
    * Scripted arrangements
    * Audio loading: `audio_create ID PATH (x,y) (width,height) [rotation DEGREES]`
* **Animation System:** Create smooth animated presentations and demos
    * Move, resize, color, rotate animations
    * Appear/disappear effects with fade in/out
    * 8 interpolation types: immediate, linear, bezier, ease-in, ease-out, bounce, elastic, back
    * Single-play or cycled loop modes
    * Precise timing control with start time and duration

### Keyboard Shortcuts

#### Content Creation
* **Ctrl+N:** Create new text
* **Ctrl+Shift+N:** Create new rich note
* **Ctrl+Shift+P:** Create new paper note
* **Ctrl+Shift+S:** Create new space

#### Navigation & Search
* **Ctrl+S:** Search elements
* **Backspace:** Go to parent space
* **Ctrl+A:** Select all elements

#### Drawing & Tools
* **Ctrl+D:** Toggle drawing mode
* **Ctrl+E:** Open DSL Executor window

#### Toolbar & Views
* **Ctrl+T:** Toggle toolbar visibility
* **Ctrl+Shift+T:** Toggle toolbar auto-hide mode
* **Ctrl+J:** Toggle space tree view

#### Editing & Clipboard
* **Ctrl+V:** Paste from clipboard
* **Ctrl+Z:** Undo action
* **Ctrl+Y:** Redo action
* **Enter:** Finish text editing
* **Delete:** Delete selected elements

## Dependencies

* GTK4
* SQLite3
* GStreamer

```bash
# UI
sudo apt install libgtk-4-dev
# Storage
sudo apt install libsqlite3-dev
# Media support (video and audio)
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
                 gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                 gstreamer1.0-libav gstreamer1.0-plugins-ugly
```

## To run on x86_64 GNU/Linux:

`make -B -j 7 && ./revel`

## How to add app launcher:

1. Copy `revel.desktop` to `~/.local/share/applications/`  
2. Update Exec and Icon path in the `.desktop` file.
3. `update-desktop-database ~/.local/share/applications/`

Be extremely concise. Sacrifice grammar for the sake of concision.
