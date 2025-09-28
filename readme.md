# Revel

Note-taking and brainstorming application with infinite canvas and powerful organization features.

## Table of Contents

* [Features](#features)
* [Dependencies](#dependencies)
* [To run on x86_64 GNU/Linux](#to-run-on-x86_64-gnulinux)
* [How to add app launcher](#how-to-add-app-launcher)

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
* **Media Notes:** Container notes that can embed images and videos
* **Shapes:** Geometric elements (circles, rectangles, triangles, diamonds, cylinders)
    * Configurable stroke width and fill options
    * Text labels with custom fonts
* **Freehand Drawing:** Free-form pen tool with adjustable stroke width
* **Images:** Direct clipboard paste and drag-and-drop support
* **Videos:** MP4 video playback with thumbnail previews

### Connections & Relationships
* **Smart Arrows:** Connect any elements with visual relationships
    * **Parallel arrows:** Curved connections with automatic routing
    * **Straight arrows:** Direct point-to-point connections
    * **Arrowhead types:** None, single, or double arrowheads
* **Connection points:** 4-point connection system for precise linking

### Element Management
* **Flexible editing:** Resize, move and style all element types
* **Element descriptions:** Private comments/metadata with creation timestamps
* **Background colors:** Customize appearance of any element
* **Z-order control:** Layer elements with bring-to-front functionality
* **Element cloning:**
    * **Fork:** Create independent copies
    * **Clone by text:** Share text content across elements
    * **Clone by size:** Share dimensions across elements

### Advanced Features
* **Comprehensive undo/redo:** Track all actions with detailed history log
* **Drag & drop:** Import images and videos directly from file manager
* **Keyboard shortcuts:** Full productivity shortcuts for all major actions
* **Font management:** Complete typography control with family, size, and style
* **Color management:** Advanced color picker for all visual elements
* **Action logging:** Timestamped history of all user actions

### User Interface
* **Context menus:** Right-click access to all element operations
* **Shape designer:** Visual shape selection and configuration dialog
* **Search interface:** Global content search across all spaces
* **Space navigation:** Easy switching between hierarchical workspaces
* **Drawing toolbar:** Tools for freehand drawing and shape creation

### Automation & Scripting
* **DSL (Domain Specific Language):** Programmatically create complex layouts
    * Batch element creation
    * Automated connections
    * Scripted arrangements

### Keyboard Shortcuts

#### Content Creation
* **Ctrl+N:** Create new rich note
* **Ctrl+Shift+P:** Create new paper note
* **Ctrl+Shift+S:** Create new space

#### Navigation & Search
* **Ctrl+S:** Search elements
* **Backspace:** Go to parent space
* **Ctrl+A:** Select all elements (when not editing)

#### Drawing & Tools
* **Ctrl+D:** Toggle drawing mode
* **Ctrl+E:** Open DSL Executor window

#### Toolbar Controls
* **Ctrl+T:** Toggle toolbar visibility
* **Ctrl+Shift+T:** Toggle toolbar auto-hide mode

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

```
# UI
sudo apt install libgtk-4-dev
# Storage
sudo apt install libsqlite3-dev
# Video support
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-libav
```

## To run on x86_64 GNU/Linux:

`make -B -j 7 && ./revel`

## How to add app launcher:

1. Copy `revel.desktop` to `~/.local/share/applications/`  
2. Update Exec and Icon path in the `.desktop` file.
3. `update-desktop-database ~/.local/share/applications/`
