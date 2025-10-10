# Revel DSL Documentation

Complete reference for Revel's Domain Specific Language (DSL) for creating complex layouts, presentations, and animations.

## Table of Contents

1. [Overview](#overview)
2. [Tutorial: Rule 110 Cellular Automaton](#tutorial-rule-110-cellular-automaton)
3. [Canvas Settings](#canvas-settings)
4. [Element Creation](#element-creation)
5. [Connections](#connections)
6. [Animation System](#animation-system)
7. [Variables & Events](#variables--events)

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

## Tutorial: Rule 110 Cellular Automaton

Let's build a complete program from scratch using `examples/rule110.dsl`. This tutorial demonstrates variables, arrays, loops, event handlers, and dynamic visualization. Rule 110 is a **Turing-complete** cellular automaton - meaning it can theoretically compute anything a traditional computer can!

### What We're Building

A 500×500 cellular automaton simulator that:
- Displays 500 cells across 500 generations
- Each cell is either alive (white) or dead (black)
- Updates based on Rule 110 pattern matching
- Runs interactively when you click START

### Step 1: Setup the Canvas

```dsl
# Dark background, no grid
canvas_background (0.05,0.05,0.08,1.0) false
```

**What you learned:**
- `canvas_background` sets background color and grid visibility
- Colors use `(r,g,b,a)` format with values 0.0-1.0
- `false` disables the grid for cleaner visualization

### Step 2: Declare Variables and Arrays

```dsl
# 500 cells - start with rightmost = 1
int c[500] 0
set c[499] 1
```

**What you learned:**
- `int c[500] 0` creates an integer array with 500 elements, all initialized to 0
- Arrays hold the current state: `c[i]` is 1 (alive) or 0 (dead)
- `set c[499] 1` sets the last cell alive (our starting seed)

```dsl
int next[500] 0
int gen 0
int step 0
int i 0
int left 0
int right 0
int pattern 0
```

**What you learned:**
- Declare all variables before use
- `next[500]` holds the next generation while computing
- `gen` tracks generation count (0-500)
- `step` controls our state machine (1=draw, 2=compute, 3=copy)
- `i`, `left`, `right`, `pattern` are temporary work variables

### Step 3: Create UI Elements

```dsl
shape_create btn rectangle "START" (400,20) (150,50) filled true bg (0.2,0.6,0.9,1.0) text_color (1.0,1.0,1.0,1.0) font "Ubuntu Bold 20"
text_create lbl "Gen: 0 / 500" (570,30) (150,30) text_color (0.8,0.8,0.8,1.0) font "Ubuntu 16"
text_create title "Rule 110 - 500x500" (50,75) (500,40) text_color (1.0,1.0,1.0,1.0) font "Ubuntu Bold 24"
```

**What you learned:**
- `shape_create ID TYPE "text" (x,y) (w,h)` creates shapes with optional text
- `filled true` fills the shape with background color
- `text_color` sets the label color inside shapes
- `text_create` makes standalone text without background
- Position elements using pixel coordinates

### Step 4: Make the Button Interactive

```dsl
on click btn
  set step 1
end
```

**What you learned:**
- `on click ELEMENT_ID` runs when element is clicked (not dragged)
- `set VARIABLE VALUE` assigns values
- Setting `step` to 1 triggers our state machine

### Step 5: Draw Current Generation (Step 1)

```dsl
on variable step == 1
  set gen {gen + 1}
  for i 0 499
    shape_create r${gen}c${i} rectangle "" ({10+i*6},{120+gen*6}) (5,5) filled true bg {c[i],c[i],c[i],1.0}
  end
  text_update lbl "Gen: ${gen} / 500"
  for i 0 499
    set next[i] 0
  end
  set step 2
end
```

**What you learned:**
- `on variable VAR == VALUE` runs every time variable equals that value
- `{expr}` evaluates expressions: `{gen + 1}`, `{10+i*6}`
- `for i START END` loops from START to END (inclusive)
- `${var}` interpolates variables in strings and element IDs
- Dynamic element IDs: `r${gen}c${i}` creates unique IDs like `r1c0`, `r1c1`, etc.
- Each cell is a 5×5 square at position `(10+i*6, 120+gen*6)`
- Grayscale color trick: `bg {c[i],c[i],c[i],1.0}` creates black (0,0,0) or white (1,1,1)
- `text_update` changes text of existing elements
- Clear `next` array before computing

### Step 6: Apply Rule 110 Logic (Step 2)

Rule 110 checks each cell and its neighbors to determine next state:

| Pattern | 111 | 110 | 101 | 100 | 011 | 010 | 001 | 000 |
|---------|-----|-----|-----|-----|-----|-----|-----|-----|
| Binary  | 7   | 6   | 5   | 4   | 3   | 2   | 1   | 0   |
| Result  | 0   | 1   | 1   | 0   | 1   | 1   | 1   | 0   |

Rule 110 = `01101110` binary = produces 1 for patterns 1,2,3,5,6

```dsl
on variable step == 2
  # Cell 0 (left edge) - assume left neighbor is 0
  set left 0
  set right {c[1]}
  set pattern {left * 4 + c[0] * 2 + right}
  set next[0] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}
```

**What you learned:**
- Handle edge cases separately (cell 0 has no left neighbor)
- `pattern = left*4 + center*2 + right` converts 3-bit binary to decimal
- Boolean trick: `(pattern == 1) + (pattern == 2) + ...` evaluates to 1 if any match, 0 otherwise
- This implements Rule 110 lookup without conditional statements

```dsl
  # Cells 1-498 (middle)
  for i 1 498
    set left {c[i - 1]}
    set right {c[i + 1]}
    set pattern {left * 4 + c[i] * 2 + right}
    set next[i] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}
  end
```

**What you learned:**
- Loop through middle cells that have both neighbors
- Expressions in array indices: `c[i - 1]`, `c[i + 1]`
- Same Rule 110 logic for all middle cells

```dsl
  # Cell 499 (right edge) - assume right neighbor is 0
  set left {c[498]}
  set right 0
  set pattern {left * 4 + c[499] * 2 + right}
  set next[499] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}

  set step 3
end
```

**What you learned:**
- Handle right edge (no right neighbor)
- Move to step 3 when computation complete

### Step 7: Copy and Continue (Step 3)

```dsl
on variable step == 3
  for i 0 499
    set c[i] {next[i]}
  end
  set step {(gen < 500) * 1}
end
```

**What you learned:**
- Copy `next` array back to `c` for next iteration
- Clever continuation: `{(gen < 500) * 1}` evaluates to 1 if gen < 500, else 0
- When step becomes 1 again, the cycle repeats (draw → compute → copy)
- When gen reaches 500, step becomes 0 and simulation stops

### The Complete State Machine

```
Click START → step=1
              ↓
         Draw cells (step 1) → increment gen → step=2
                                                  ↓
                                        Compute Rule 110 (step 2) → step=3
                                                                       ↓
                                                       Copy next→c (step 3) → step=1 or 0
                                                                                ↓
                                                                          (repeat or stop)
```

### Key Concepts Summary

1. **Arrays**: `int c[500] 0` declares and initializes array
2. **Loops**: `for i 0 499` iterates with inclusive bounds
3. **Expressions**: `{gen + 1}`, `{10+i*6}` compute values dynamically
4. **Interpolation**: `${gen}` in strings, `r${gen}c${i}` in IDs
5. **Event Handlers**: `on click`, `on variable` trigger actions
6. **State Machine**: Use integer variable to control program flow
7. **Boolean Math**: `(condition) + (condition)` counts true conditions
8. **Dynamic Creation**: Generate elements in loops with unique IDs
9. **Array Access**: Use variables as indices: `c[i]`, `next[i]`

### Running the Example

```bash
./revel --dsl examples/rule110.dsl
```

Click START and watch Rule 110 generate 500 generations of Turing-complete patterns!

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
- `ease-in` - Starts slow, speeds up
- `ease-out` - Starts fast, slows down
- `bounce` - Bouncing effect at the end
- `elastic` - Spring-like effect
- `back` - Overshoots then returns

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
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

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
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

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
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

### Rotate Animation

Animate element rotation.

```
animate_rotate ELEMENT_ID FROM_DEGREES TO_DEGREES START_TIME DURATION [TYPE]
animate_rotate ELEMENT_ID TO_DEGREES START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `FROM_DEGREES` - Starting rotation angle in degrees (optional, uses current rotation if omitted)
- `TO_DEGREES` - Ending rotation angle in degrees
- `START_TIME` - When to start (seconds)
- `DURATION` - Animation duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

**Examples:**
```dsl
# Rotate from 0 to 360 degrees
animate_rotate star 0 360 0.0 2.0 linear

# Rotate from current angle to 180 degrees
animate_rotate box 180 1.0 1.5 bezier
```

### Appear Animation

Fade in element from transparent to visible.

```
animate_appear ELEMENT_ID START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `START_TIME` - When to start (seconds)
- `DURATION` - Fade-in duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

### Disappear Animation

Fade out element from visible to transparent.

```
animate_disappear ELEMENT_ID START_TIME DURATION [TYPE]
```

**Parameters:**
- `ELEMENT_ID` - ID of element to animate
- `START_TIME` - When to start (seconds)
- `DURATION` - Fade-out duration in seconds
- `TYPE` - Interpolation type (optional): `immediate`, `linear`, `bezier`, `ease-in`, `ease-out`, `bounce`, `elastic`, `back`

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

You can also advance to the next slide programmatically - for example, call `presentation_next` inside an `on variable` handler once a learner submits the correct answer.

**Example Files:**
- `examples/presentation_demo.dsl` - Basic presentation without animations
- `examples/interactive_dashboard.dsl` - Variable-driven sales dashboard with animated bars
- `examples/interactive_gauge.dsl` - Clickable thermostat gauge with live readouts
- `examples/quiz_match.dsl` - Interactive quiz showing auto-advance from animations and variable triggers
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

## Variables & Events

Create dynamic layouts by storing values, reacting to user interaction, and updating visuals automatically.

### Declare Variables

```
int    NAME VALUE
real   NAME VALUE
bool   NAME true|false
string NAME "Text"
int    NAME[SIZE] INITIAL_VALUE
real   NAME[SIZE] INITIAL_VALUE
global int    NAME VALUE
global real   NAME VALUE
global bool   NAME true|false
global string NAME "Text"

int total_sales {q1 + q2 + q3 + q4}
bool is_hot {current_temp - target_temp}
string status "Total: ${total_sales}"
int cells[60] 0
real prices[10] 99.99
```

- Numeric declarations (`int`, `real`) accept literal values or expressions inside `{ ... }`
- Boolean declarations accept literals (`true`, `false`, `yes`, `no`, `1`, `0`) or expressions (non-zero evaluates to `true`)
- Strings use quoted text; `${ ... }` interpolation works when rendering
- **Arrays**: Declare with `int NAME[SIZE] INITIAL_VALUE` or `real NAME[SIZE] INITIAL_VALUE` - all elements initialized to the same value
- Array access: Use `NAME[index]` where index can be a literal, variable, or expression `{expr}`
- Prefix declarations with `global` when the value should persist across presentation slides; globals keep their value between `animation_next_slide` executions and are not cleared when moving forward/backward.

### Interpolate Values

```
text_create total "Total: ${total_sales}" (400,120) (320,40)
```

- `${ ... }` in strings evaluates expressions when the element is created or when refreshed by events

### Control Flow (inside event blocks)

```
for VARIABLE START END
  # commands
end
```

- Loops from START to END (inclusive)
- VARIABLE is automatically created as `int` if not declared
- START and END can be literals, variables, or expressions
- Can be nested (for within for)
- Supported inside `on click` and `on variable` event blocks

**Example:**
```
for i 0 59
  set cells[i] 0
end

for row 0 9
  for col 0 9
    shape_create cell${row}${col} rectangle "" ({col*50},{row*50}) (48,48) filled true
  end
end
```

### Runtime Commands (inside event blocks)

```
set VARIABLE VALUE_EXPR         # set variable to expression result
set ARRAY[INDEX] VALUE_EXPR     # set array element
animate_move ELEMENT (to_x,to_y) START DURATION [interp]
animate_resize ELEMENT (to_w,to_h) START DURATION [interp]
text_update ELEMENT "New text with ${expr}"
text_bind ELEMENT_ID VARIABLE   # bind a text element to a string variable
position_bind ELEMENT_ID VARIABLE # store element position as "x,y" string
presentation_next               # advance presentation to next slide
presentation_auto_next_if VARIABLE VALUE  # auto-advance when variable reaches value
```

`text_bind` keeps the element text and string variable in sync - when the user finishes editing the bound element, the variable is updated and any `on variable` handlers run. `position_bind` updates the referenced string variable with the element's canvas coordinates (e.g. `"420,180"`) whenever the element is moved, which is useful for drag/match activities.

`presentation_auto_next_if` lets you automatically advance to the next presentation slide once a variable reaches a target value (numeric or string). This is handy for quizzes - set a counter or status variable in response to user actions, then register the auto-advance trigger.

`set` assigns values to variables or array elements. Declare all variables with `int`, `real`, `bool`, `string`, or array types before using them.

### Event Handlers

```
on click element_id
  add counter 1
end

on variable counter
  text_update label "Clicks: ${counter}"
end

on variable step == 5
  set next_step {step + 1}
end

on variable temp < 60
  text_update status "Too cold!"
end

on variable temp > 80
  text_update status "Too hot!"
end
```

- `on click ELEMENT_ID` runs when the element is clicked (and not dragged)
- `on variable VAR_NAME` runs **every time** the named variable changes (any change triggers)
- `on variable VAR_NAME == VALUE` runs **every time** variable equals VALUE when it changes
- `on variable VAR_NAME < VALUE` runs **every time** variable is less than VALUE when it changes
- `on variable VAR_NAME > VALUE` runs **every time** variable is greater than VALUE when it changes
- `on variable VAR_NAME <= VALUE` runs **every time** variable is less than or equal to VALUE when it changes
- `on variable VAR_NAME >= VALUE` runs **every time** variable is greater than or equal to VALUE when it changes
- `on variable VAR_NAME != VALUE` runs **every time** variable is not equal to VALUE when it changes
- Handlers may contain multiple commands including `for` loops
- Animations are queued automatically
- **IMPORTANT**:
  - Cannot nest `on variable` inside another `on variable` - all event handlers must be top-level
  - Handlers execute **every time** the condition is met, not just once
  - If `set step {step + 1}` triggers `on variable step == 4`, that handler runs every time step becomes 4
