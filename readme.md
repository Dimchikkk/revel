# Revel

Note-taking and brainstorming application with infinite canvas and powerful organization features.

## Table of Contents

* [Features](#features)
* [Dependencies](#dependencies)
* [To run on x86_64 GNU/Linux](#to-run-on-x86_64-gnulinux)
* [Project layout](#project-layout)
* [DSL example](#dsl-example)
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
* **Shapes:** Geometric elements with extensive customization options
    * **Types:** Circles, rectangles, rounded rectangles, triangles, diamonds, cylinders, arrows, and lines
    * **Stroke styles:** Solid, dashed, and dotted lines with configurable width
    * **Fill patterns:** Solid fills, hachure, and cross-hatch patterns
    * **Independent colors:** Separate fill and stroke color control
    * **Text labels:** Custom fonts and colors within shapes
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
* **Space tree view:** Hierarchical tree view of all spaces with navigation
    * Toggle visibility with dedicated toolbar button
    * Quick navigation by clicking on any space
* **Drawing toolbar:** Tools for freehand drawing and shape creation

### Automation & Scripting
* **DSL (Domain Specific Language):** Programmatically create complex layouts
    * Batch element creation
    * Automated connections
    * Scripted arrangements

### Keyboard Shortcuts

Press `F1` inside the app to open an in-product reference with every shortcut.

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

`make -j 7 && ./revel`

## Project layout

```
src/              Core application sources and headers
tests/            Unit and integration test suites
examples/         Sample DSL scripts and their media assets
build/            Generated object files and test runners (created by make)
```

## DSL example

An end-to-end DSL walkthrough is provided in `examples/showcase.dsl`. It
demonstrates notes, shapes, media, and styled connections and uses the sample
assets bundled with the repository.

Run it from the project root:

```
make -j 7
rm -f demo.db && ./revel --dsl examples/showcase.dsl demo.db
```

This command generates `demo.db` alongside the executable. Launching `revel` and
opening the generated database will display the scripted layout shown below:

![Demo Canvas](examples/media/demo.jpg)

## How to add app launcher:

1. Copy `revel.desktop` to `~/.local/share/applications/`  
2. Update Exec and Icon path in the `.desktop` file.
3. `update-desktop-database ~/.local/share/applications/`
