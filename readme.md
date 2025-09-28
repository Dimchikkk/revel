# Revel

Note-taking and brainstorming app

## Table of Contents

* [Features](#features)
* [Dependencies](#dependencies)
* [To run on x86_64 GNU/Linux](#to-run-on-x86_64-gnulinux)
* [How to add app launcher](#how-to-add-app-launcher)



## Features:

### General
* All data is stored in a single portable SQLite3 database file.
* Full-text search with BM25 ranking.
* Infinite canvas with zoom and pan.
* Organize elements into nested spaces (infinite space depth).
* Move elements between spaces.
* Undo/redo for most actions.
* Customizable canvas background color and grid.

### Elements
* Create different types of notes:
    * **Paper Notes:** Simple text notes.
    * **Notes:** Text notes with more formatting options.
    * **Media Notes:** Notes that can contain images and videos.
* **Images:** Paste images from clipboard and annotate them.
* **Videos:** Store and play short MP4 clips.
* **Shapes:** Draw various geometrical shapes like circles, rectangles, triangles, and more.
* **Freehand Drawing:** Draw on the canvas with a pen tool.
* Connect elements with arrows.
* Resize, move, delete, and change the background color of elements.
* Forking an element creates an independent copy.
* Cloning an element creates a copy with properties that stay in sync.

### Text and Fonts
* Change text properties like font, color, size, and style.

### Scripting
* Execute a simple DSL to programmatically create notes and connections.

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