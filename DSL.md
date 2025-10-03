# Revel DSL Documentation

Complete reference for Revel's Domain Specific Language (DSL) for creating complex layouts, presentations, and animations.

## Table of Contents

1. [Overview](#overview)
2. [Canvas Settings](#canvas-settings)
3. [Element Creation](#element-creation)
4. [Connections](#connections)
5. [Animation System](#animation-system)
6. [Examples](#examples)

---

## Overview

Revel DSL allows you to programmatically create complex layouts with notes, shapes, media, and connections. All DSL scripts can be executed via the DSL Executor window (**Ctrl+E**).

### General Syntax Rules

- One command per line
- Lines starting with `#` are comments
- Empty lines are ignored
- Color formats: `(r,g,b,a)`, `#RRGGBB`, `#RRGGBBAA`, or `color(r,g,b,a)`
  - RGB values: 0.0-1.0 for `(r,g,b,a)` format, 0-255 for hex
  - Alpha: 0.0-1.0 (optional, defaults to 1.0)
- Position/size format: `(x,y)` or `(width,height)` in pixels
- Text strings: Use `"quotes"` for multi-word text
- Escape sequences in text: `\n` (newline), `\t` (tab), `\"` (quote), `\\` (backslash)

---

## Canvas Settings

### Canvas Background

```
canvas_background (bg_r,bg_g,bg_b,bg_a) SHOW_GRID (grid_r,grid_g,grid_b,grid_a)
```

**Parameters:**
- `(bg_r,bg_g,bg_b,bg_a)` - Background color
- `SHOW_GRID` - `true` or `false` to show/hide grid
- `(grid_r,grid_g,grid_b,grid_a)` - Grid line color (optional)

**Example:**
```
canvas_background (0.15,0.15,0.18,1.0) true (0.25,0.25,0.30,0.5)
```

---

## Element Creation

### Text Notes

#### Regular Note
```
note_create ID "Text" (x,y) (w,h) [OPTIONS]
```

#### Paper Note (Yellow sticky note style)
```
paper_note_create ID "Text" (x,y) (w,h) [OPTIONS]
```

#### Standalone Text (No background)
```
text_create ID "Text" (x,y) (w,h) [OPTIONS]
```

**Common Options:**
- `bg <color>` or `background <color>` - Background color
- `text_color <color>` or `text <color>` - Text color
- `font "Font Name Size"` - Font (e.g., `"Ubuntu Bold 24"`)
- `align ALIGNMENT` - Text alignment
- `rotation DEGREES` - Rotation angle (0-360)

**Alignment Values:**
- `top-left`, `top-center`, `top-right`
- `center` (default)
- `bottom-left`, `bottom-right`

**Examples:**
```
note_create title "Revel" (50,50) (280,70) bg #2563eb text_color #FFFFFF font "Ubuntu Bold 36"
paper_note_create note1 "Quick idea" (100,200) (200,100)
text_create label "Caption" (50,400) (150,30) text_color #94a3b8 font "Ubuntu 13"
```

### Shapes

```
shape_create ID TYPE "Text" (x,y) (w,h) [OPTIONS]
```

**Shape Types:**
- `circle` - Circle
- `rectangle` - Rectangle
- `roundedrect` - Rounded rectangle
- `triangle` - Triangle
- `diamond` - Diamond
- `vcylinder` - Vertical cylinder
- `hcylinder` - Horizontal cylinder
- `line` - Line
- `arrow` - Arrow
- `bezier` - Bezier curve

**Shape Options:**
- `bg <color>` - Fill/background color
- `stroke <width>` - Stroke width in pixels
- `stroke_color <color>` - Stroke color
- `stroke_style STYLE` - Stroke style: `solid`, `dashed`, `dotted`
- `filled <bool>` - `true` or `false` for filled shapes
- `fill_style STYLE` - Fill pattern: `solid`, `hachure`, `crosshatch`
- `text_color <color>` - Label text color
- `font "Font Name"` - Font for label text
- `align ALIGNMENT` - Text alignment within shape
- `rotation DEGREES` - Rotation angle

**Line/Arrow Specific Options:**
- `line_start (u,v)` - Start point (u,v are 0.0-1.0 relative to shape bounds)
- `line_end (u,v)` - End point (u,v are 0.0-1.0 relative to shape bounds)

**Bezier Specific Options:**
- `p0 (u,v)` - Control point 0 (relative 0.0-1.0)
- `p1 (u,v)` - Control point 1
- `p2 (u,v)` - Control point 2
- `p3 (u,v)` - Control point 3

**Examples:**
```
shape_create s1 circle "Circle" (50,200) (120,120) bg #10b981 stroke 3 stroke_color #065f46 filled true
shape_create s2 rectangle "Box" (200,200) (130,80) bg #f59e0b filled true fill_style hachure
shape_create s3 roundedrect "Rounded" (360,200) (130,80) bg #8b5cf6 stroke 3 filled true
shape_create arrow1 arrow "" (900,50) (180,40) bg #ef4444 stroke 3 stroke_style dashed rotation 15
shape_create line1 line "" (100,100) (200,20) bg #8b5cf6 stroke 2 stroke_style dotted
```

### Media

#### Images
```
image_create ID /absolute/path/to/image.png (x,y) (w,h) [rotation DEGREES]
```

**Supported formats:** PNG, JPEG, GIF, BMP, SVG

#### Videos
```
video_create ID /absolute/path/to/video.mp4 (x,y) (w,h) [rotation DEGREES]
```

**Supported formats:** MP4, WebM, OGG (via GStreamer)

**Examples:**
```
image_create img1 /home/user/photos/cat.jpeg (50,410) (200,200)
video_create vid1 /home/user/videos/demo.mp4 (280,410) (320,200) rotation 10
```

### Spaces

Create nested workspaces (sub-canvases):

```
space_create ID "Name" (x,y) (w,h) [rotation DEGREES]
```

**Example:**
```
space_create workspace1 "Project Ideas" (100,100) (300,200)
```

---

## Connections

Create arrows connecting elements:

```
connect FROM_ID TO_ID [TYPE] [ARROWHEAD] [COLOR]
```

**Parameters:**
- `FROM_ID` - Source element ID
- `TO_ID` - Target element ID
- `TYPE` - Connection type (optional):
  - `parallel` - Curved connection with auto-routing (default)
  - `straight` - Direct straight line
- `ARROWHEAD` - Arrowhead style (optional):
  - `none` - No arrowhead
  - `single` - Single arrowhead at target (default)
  - `double` - Arrowheads at both ends
- `COLOR` - Connection color (optional, defaults to white)

**Examples:**
```
connect title section1 parallel single #3b82f6
connect section1 section2 straight none #64748b80
connect box1 box2 parallel double (0.5,0.8,0.3,1.0)
```

---

## Animation System

Create animated presentations and demonstrations using animation mode. All animations support three interpolation types for smooth motion.

**Syntax Philosophy:**
- **Element creation** uses optional keywords (`bg`, `text_color`, `filled`, etc.) because properties can appear in any order
- **Animations** use positional parameters (from → to → when → duration → how) because the order is always logical and consistent

**Example:**
```dsl
# Element with optional keyword parameters
shape_create star circle "⭐" (100,100) (60,60) bg #ffd700 filled true stroke 2

# Animation with intuitive positional parameters
animate_move star (100,100) (500,300) 0.0 2.0 bezier
```

### Enable Animation Mode

```
animation_mode          # Single playback - plays once
animation_mode cycled   # Looping playback - repeats forever
```

### Interpolation Types

All animations support these interpolation types:
- `immediate` - Instant jump (no interpolation)
- `linear` - Constant speed (default)
- `bezier` - Smooth ease-in-out curve

### Move Animation

Animate element position changes.

```
animate_move ELEMENT_ID (from_x,from_y) (to_x,to_y) START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `(from_x,from_y)` - Starting position in pixels
- `(to_x,to_y)` - Ending position in pixels
- `START_TIME` - When to start (seconds from animation start)
- `DURATION` - Animation duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`

### Resize Animation

Animate element size changes.

```
animate_resize ELEMENT_ID (from_w,from_h) (to_w,to_h) START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `(from_w,from_h)` - Starting size in pixels
- `(to_w,to_h)` - Ending size in pixels
- `START_TIME` - When to start (seconds)
- `DURATION` - Animation duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`

### Color Animation

Animate background color changes.

```
animate_color ELEMENT_ID FROM_COLOR TO_COLOR START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `FROM_COLOR` - Starting color (formats: `(r,g,b,a)`, `#RRGGBB`, `#RRGGBBAA`)
- `TO_COLOR` - Ending color
- `START_TIME` - When to start (seconds)
- `DURATION` - Animation duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`

### Appear Animation

Fade in element from transparent to visible.

```
animate_appear ELEMENT_ID START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `START_TIME` - When to start (seconds)
- `DURATION` - Fade-in duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`

### Disappear Animation

Fade out element from visible to transparent.

```
animate_disappear ELEMENT_ID START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `START_TIME` - When to start (seconds)
- `DURATION` - Fade-out duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`

**Examples:**

#### Basic Movement
```
animation_mode

note_create ball "Ball" (100,100) (60,60) bg #ff6b00
animate_move ball (100,100) (500,300) 0.0 2.0 linear
```

#### Resize Animation
```
animation_mode

note_create box "Growing Box" (200,200) (50,50) bg #3b82f6
animate_resize box (50,50) (200,200) 0.0 1.5 bezier
```

#### Color Animation
```
animation_mode

note_create chameleon "Color Change" (300,100) (180,80) bg #ef4444
animate_color chameleon #ef4444 #10b981 0.0 2.0 bezier
```

#### Appear/Disappear
```
animation_mode

note_create ghost "I appear..." (200,150) (150,60) bg #8b5cf6
note_create phantom "...and disappear" (200,250) (150,60) bg #f59e0b

animate_appear ghost 0.0 1.0 bezier
animate_disappear phantom 2.0 1.0 bezier
```

#### Staggered Animations
```
animation_mode

note_create elem1 "First" (100,100) (80,40)
note_create elem2 "Second" (100,200) (80,40)
note_create elem3 "Third" (100,300) (80,40)

# Start at different times
animate_move elem1 (100,100) (500,100) 0.0 1.0 bezier
animate_move elem2 (100,200) (500,200) 0.5 1.0 bezier
animate_move elem3 (100,300) (500,300) 1.0 1.0 bezier
```

#### Looping Animation
```
animation_mode cycled

note_create spinner "•" (300,300) (40,40) bg #2563eb

# Create circular motion
animate_move spinner (300,300) (400,300) 0.0 0.25 bezier
animate_move spinner (400,300) (400,400) 0.25 0.25 bezier
animate_move spinner (400,400) (300,400) 0.5 0.25 bezier
animate_move spinner (300,400) (300,300) 0.75 0.25 bezier
```

#### Complex Sequence
```
animation_mode

# Title appears immediately
note_create title "Presentation" (300,50) (400,60) bg #1e3a8a text_color #FFFFFF font "Ubuntu Bold 32"
animate_move title (300,-100) (300,50) 0.0 0.8 bezier

# Bullet points fade in sequentially
note_create bullet1 "• First Point" (100,150) (300,40)
note_create bullet2 "• Second Point" (100,220) (300,40)
note_create bullet3 "• Third Point" (100,290) (300,40)

animate_move bullet1 (-200,150) (100,150) 1.0 0.6 bezier
animate_move bullet2 (-200,220) (100,220) 1.5 0.6 bezier
animate_move bullet3 (-200,290) (100,290) 2.0 0.6 bezier

# Diagram appears from right
shape_create diagram circle "Summary" (600,200) (150,150) bg #10b981 stroke 3 filled true
animate_move diagram (1000,200) (600,200) 2.5 0.8 bezier
```

---

## Examples

### Basic Layout

```
# Simple note-taking layout
canvas_background (0.09,0.09,0.09,1.0) true (0.2,0.2,0.2,0.5)

note_create header "My Notes" (50,50) (300,60) bg #2563eb text_color #FFFFFF font "Ubuntu Bold 28"
paper_note_create note1 "Todo:\n- Buy groceries\n- Call mom" (50,150) (200,120)
paper_note_create note2 "Ideas for project" (280,150) (200,120)

connect note1 note2 parallel single #64748b
```

### Shape Showcase

```
canvas_background (0.15,0.15,0.18,1.0) false

# Different shapes with various styles
shape_create c1 circle "Circle" (50,50) (100,100) bg #10b981 stroke 3 filled true
shape_create r1 rectangle "Rectangle" (180,50) (120,80) bg #f59e0b filled true fill_style hachure
shape_create d1 diamond "Diamond" (330,50) (90,90) bg #8b5cf6 stroke 2 filled true
shape_create t1 triangle "Triangle" (450,50) (100,100) bg #ec4899 filled true fill_style crosshatch

# Arrows and lines
shape_create a1 arrow "" (50,200) (150,30) bg #ef4444 stroke 3 stroke_style solid
shape_create a2 arrow "" (50,250) (150,30) bg #3b82f6 stroke 3 stroke_style dashed
shape_create l1 line "" (50,300) (150,20) bg #8b5cf6 stroke 2 stroke_style dotted
```

### Media Gallery

```
canvas_background (0.1,0.1,0.1,1.0) false

text_create title "Media Gallery" (50,20) (600,40) text_color #FFFFFF font "Ubuntu Bold 24"

image_create photo1 /home/user/photos/vacation.jpg (50,80) (250,200)
image_create photo2 /home/user/photos/family.jpg (320,80) (250,200)
video_create clip1 /home/user/videos/demo.mp4 (50,300) (520,300)

text_create caption1 "Summer 2024" (50,290) (250,20) text_color #94a3b8
text_create caption2 "Family Reunion" (320,290) (250,20) text_color #94a3b8
```

### Presentation with Animation

```
animation_mode

# Setup
canvas_background (0.05,0.05,0.1,1.0) false

# Title
note_create title "Product Roadmap" (250,50) (500,80) bg #1e3a8a text_color #FFFFFF font "Ubuntu Bold 36"

# Phases
shape_create phase1 roundedrect "Phase 1\nResearch" (100,200) (180,120) bg #10b981 stroke 2 filled true text_color #FFFFFF
shape_create phase2 roundedrect "Phase 2\nDevelopment" (320,200) (180,120) bg #f59e0b stroke 2 filled true text_color #FFFFFF
shape_create phase3 roundedrect "Phase 3\nLaunch" (540,200) (180,120) bg #ef4444 stroke 2 filled true text_color #FFFFFF

# Connections
connect phase1 phase2 parallel single #FFFFFF
connect phase2 phase3 parallel single #FFFFFF

# Animate title dropping in
animate_move title (250,-100) (250,50) 0.0 0.8 bezier

# Phases appear sequentially from bottom
animate_move phase1 (100,600) (100,200) 1.0 0.6 bezier
animate_move phase2 (320,600) (320,200) 1.4 0.6 bezier
animate_move phase3 (540,600) (540,200) 1.8 0.6 bezier
```

### Looping Demo

```
animation_mode cycled

# Background
canvas_background (0.05,0.05,0.1,1.0) true (0.15,0.15,0.2,0.3)

# Create a bouncing ball
shape_create ball circle "" (400,100) (60,60) bg #ff6b00 filled true

# Ground indicator
shape_create ground rectangle "" (50,450) (700,10) bg #334155 filled true

# Bounce animation
animate_move ball (400,100) (400,370) 0.0 0.8 bezier  # Fall down
animate_move ball (400,370) (400,100) 0.8 0.8 bezier  # Bounce up
```

---

## Tips and Best Practices

1. **Element IDs**: Use descriptive IDs for easier reference in connections and animations
2. **Color Consistency**: Use a consistent color palette for professional-looking layouts
3. **Layering**: Elements created later appear on top of earlier elements
4. **Animations**: Start with simple animations, then add complexity
5. **Performance**: For large layouts (1000+ elements), rendering may take time
6. **Export to DSL**: Use the "Export to DSL" button in DSL Executor to generate DSL from current canvas
7. **Animation Recording**: Use screen recording tools to capture cycled animations as videos/GIFs

---

## Keyboard Shortcuts

- **Ctrl+E** - Open DSL Executor window
- **Ctrl+S** - Search elements
- **Ctrl+Z** / **Ctrl+Y** - Undo / Redo

---

## See Also

- `examples/showcase.dsl` - Complete showcase of all DSL features
- `examples/animation_example.dsl` - Basic animation demonstration (3 moving circles)
- `examples/animation_bouncing_ball.dsl` - Looping animation example
- `examples/animation_elegant_demo.dsl` - **Complete animation demo** showing all features
