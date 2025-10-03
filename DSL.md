# Revel DSL Documentation

Complete reference for Revel's Domain Specific Language (DSL) for creating complex layouts, presentations, and animations.

## Table of Contents

1. [Overview](#overview)
2. [Canvas Settings](#canvas-settings)
3. [Element Creation](#element-creation)
4. [Connections](#connections)
5. [Animation System](#animation-system)

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

---

## Presentation Mode

Create slide-based presentations with automatic space clearing between slides.

### Slide Separator

```
animation_next_slide
```

**Description:**
- Marks the boundary between slides in a presentation
- When reached, pauses execution and waits for navigation input
- Clears all elements from current space (preserving grid settings) before moving to next slide
- Script is automatically split into slides at each `animation_next_slide` command

### Navigation Controls

- **Ctrl+Right Arrow** - Move to next slide
- **Ctrl+Left Arrow** - Move to previous slide

### Example Presentation

```dsl
# Slide 1: Title
canvas_background (0.1,0.1,0.15,1.0) true (0.2,0.2,0.25,0.5)
text_create title "Welcome to Revel" (400,300) (400,100) text_color #FFFFFF font "Ubuntu Bold 48"

animation_next_slide

# Slide 2: Feature List
text_create header "Key Features" (400,150) (400,80) text_color #3b82f6 font "Ubuntu Bold 36"
note_create feat1 "• Infinite canvas" (100,300) (300,60)
note_create feat2 "• Rich animations" (100,380) (300,60)
note_create feat3 "• Nested spaces" (100,460) (300,60)

animation_next_slide

# Slide 3: Conclusion
text_create thanks "Thank You!" (400,350) (300,80) text_color #10b981 font "Ubuntu Bold 42"
```

**Usage:**
1. Open DSL Executor (**Ctrl+E**)
2. Enter your presentation script with `animation_next_slide` separators
3. Execute the script - first slide displays automatically
4. Use **Ctrl+Right Arrow** to advance, **Ctrl+Left Arrow** to go back
5. Optionally hide toolbar with **Ctrl+Shift+T** for clean presentation view
6. Notifications appear when trying to navigate past first or last slide

**Combining with Animations:**

Presentations can include animations on each slide:

```dsl
# Slide with single-play animation
animation_mode
canvas_background (0.1,0.1,0.15,1.0) true
text_create title "Animated Slide" (300,200) (400,100) text_color #FFFFFF
animate_appear title 0.0 1.5 bezier

animation_next_slide

# Slide with looping animation
animation_mode cycled
text_create pulse "Pulsing Text" (300,300) (400,100) text_color #60a5fa
shape_create circle1 circle "" (500,350) (80,80) bg #f59e0b filled true
animate_resize circle1 (80,80) (120,120) 0.0 1.0 bezier
animate_resize circle1 (120,120) (80,80) 1.0 1.0 bezier
```

**Example Files:**
- `examples/presentation_demo.dsl` - Basic presentation without animations
- `examples/presentation_with_animation.dsl` - Presentation with single-play animations
- `examples/presentation_with_cycled_animation.dsl` - Presentation with looping animations

---

## Tips and Best Practices

1. **Element IDs**: Use descriptive IDs for easier reference in connections and animations
2. **Color Consistency**: Use a consistent color palette for professional-looking layouts
3. **Layering**: Elements created later appear on top of earlier elements
4. **Animations**: Start with simple animations, then add complexity
5. **Performance**: For large layouts (1000+ elements), rendering may take time
6. **Export to DSL**: Use the "Export to DSL" button in DSL Executor to generate DSL from current canvas
7. **Animation Recording**: Use screen recording tools to capture cycled animations as videos/GIFs
