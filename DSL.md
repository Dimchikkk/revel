# Revel DSL Documentation

Complete reference for Revel's Domain Specific Language (DSL) for creating complex layouts, presentations, and animations.

## Execution

DSL scripts can be executed in two ways:
- **DSL Executor window**: Press **Ctrl+E** to open the interactive executor
- **Command line**: `./revel --dsl path/to/script.dsl` to run scripts directly on startup

## AI Assistant Usage

Open the AI Assistant from the toolbar `AI` toggle. The assistant captures the
current canvas as DSL, truncating context to the configured byte budget, and
issues the prompt to the selected CLI provider. Returned scripts are validated
with the DSL type checker; if validation fails the assistant retries up to
three times while sharing the failure context. All successful and failed
responses are logged in the SQLite action log with `origin="ai"`.

Use the gear button inside the dialog to customise provider path overrides,
timeouts, context size, and grammar snippet inclusion. Leave the CLI path blank
to fall back to the default executable name from `config/ai_providers.json`.

## Table of Contents

1. [Grammar Reference](#grammar-reference)
2. [Tutorial: Rule 110 Cellular Automaton](#tutorial-rule-110-cellular-automaton)
3. [Canvas Settings](#canvas-settings)
4. [Element Creation](#element-creation)
5. [Connections](#connections)
6. [Animation System](#animation-system)
7. [Variables & Events](#variables--events)

---

## Grammar Reference

Complete formal syntax reference for Revel DSL.

### Lexical Structure

```
COMMENT    ::= '#' .* NEWLINE
WHITESPACE ::= ' ' | '\t'
NEWLINE    ::= '\n'
IDENTIFIER ::= [a-zA-Z_][a-zA-Z0-9_]*
NUMBER     ::= [0-9]+ ('.' [0-9]+)?
STRING     ::= '"' (CHAR | ESCAPE)* '"'
ESCAPE     ::= '\n' | '\t' | '\"' | '\\'
POINT      ::= '(' EXPR ',' EXPR ')'
COLOR      ::= '(' EXPR ',' EXPR ',' EXPR ',' EXPR ')'
            | '#' [0-9A-Fa-f]{6,8}
EXPR       ::= '{' EXPRESSION '}'
TEXT_INTERP::= '${' EXPRESSION '}'
```

### Top-Level Declarations

```
Program ::= (Statement | Comment | EmptyLine)*

Statement ::= VariableDecl
           | ElementCreate
           | Connection
           | CanvasConfig
           | AnimationConfig
           | ForLoop
           | EventHandler

VariableDecl ::= ['global'] VarType IDENTIFIER Value
              | ['global'] VarType IDENTIFIER '[' NUMBER ']' Value

VarType ::= 'int' | 'real' | 'bool' | 'string'
Value   ::= NUMBER | EXPR | STRING | BoolLiteral
BoolLiteral ::= 'true' | 'false' | 'yes' | 'no' | '1' | '0'
```

### Element Creation

```
ElementCreate ::= NoteCreate
               | ShapeCreate
               | MediaCreate
               | SpaceCreate

NoteCreate ::= ('note_create' | 'paper_note_create' | 'text_create')
               IDENTIFIER STRING POINT POINT [Options]*

ShapeCreate ::= 'shape_create' IDENTIFIER ShapeType STRING
                POINT POINT [Options]*

ShapeType ::= 'circle' | 'rectangle' | 'roundedrect' | 'triangle'
           | 'diamond' | 'vcylinder' | 'hcylinder'
           | 'line' | 'arrow' | 'bezier'

MediaCreate ::= ('image_create' | 'video_create' | 'audio_create')
                IDENTIFIER PATH POINT POINT ['rotation' NUMBER]

SpaceCreate ::= 'space_create' IDENTIFIER STRING POINT POINT
                ['rotation' NUMBER]

Options ::= 'bg' COLOR
         | 'text_color' COLOR
         | 'font' STRING
         | 'align' Alignment
         | 'rotation' NUMBER
         | 'filled' BoolLiteral
         | 'stroke' NUMBER
         | 'stroke_color' COLOR
         | 'stroke_style' StrokeStyle
         | 'fill_style' FillStyle
         | 'line_start' POINT
         | 'line_end' POINT
         | 'p0' POINT | 'p1' POINT | 'p2' POINT | 'p3' POINT

Alignment ::= 'top-left' | 'top-center' | 'top-right'
           | 'center' | 'bottom-left' | 'bottom-right'

StrokeStyle ::= 'solid' | 'dashed' | 'dotted'
FillStyle   ::= 'solid' | 'hachure' | 'crosshatch'
```

### Connections

```
Connection ::= 'connect' IDENTIFIER IDENTIFIER
               [ConnType] [ArrowHead] [COLOR]

ConnType   ::= 'parallel' | 'straight'
ArrowHead  ::= 'none' | 'single' | 'double'
```

### Canvas & Animation Configuration

```
CanvasConfig ::= 'canvas_background' COLOR BoolLiteral [COLOR]

AnimationConfig ::= 'animation_mode' ['cycled']
                 | 'animation_next_slide'
```

### Control Flow

```
ForLoop ::= 'for' IDENTIFIER EXPR EXPR
            Statement*
            'end'
```

### Event Handlers

```
EventHandler ::= OnClick | OnVariable

OnClick ::= 'on' 'click' IDENTIFIER
            RuntimeCommand*
            'end'

OnVariable ::= 'on' 'variable' IDENTIFIER [Comparator Value]
               RuntimeCommand*
               'end'

Comparator ::= '==' | '!=' | '<' | '>' | '<=' | '>='

RuntimeCommand ::= VariableDecl
                | SetCommand
                | AnimateCommand
                | TextCommand
                | BindCommand
                | PresentationCommand
                | ForLoop
                | ElementCreate

SetCommand ::= 'set' VarAccess EXPR
VarAccess  ::= IDENTIFIER ['[' EXPR ']']

AnimateCommand ::= 'animate_move' IDENTIFIER [POINT] POINT NUMBER NUMBER [InterpType]
                | 'animate_resize' IDENTIFIER [POINT] POINT NUMBER NUMBER [InterpType]
                | 'animate_color' IDENTIFIER COLOR COLOR NUMBER NUMBER [InterpType]
                | 'animate_rotate' IDENTIFIER [NUMBER] NUMBER NUMBER NUMBER [InterpType]
                | 'animate_appear' IDENTIFIER NUMBER NUMBER [InterpType]
                | 'animate_disappear' IDENTIFIER NUMBER NUMBER [InterpType]

InterpType ::= 'immediate' | 'linear' | 'bezier' | 'ease-in' | 'ease-out'
            | 'bounce' | 'elastic' | 'back'

TextCommand ::= 'text_update' IDENTIFIER STRING

BindCommand ::= 'text_bind' IDENTIFIER IDENTIFIER
             | 'position_bind' IDENTIFIER IDENTIFIER

PresentationCommand ::= 'presentation_next'
                     | 'presentation_auto_next_if' IDENTIFIER Value
```

### Expression Language

```
EXPRESSION ::= Primary
            | EXPRESSION BinOp EXPRESSION
            | UnaryOp EXPRESSION
            | '(' EXPRESSION ')'
            | FunctionCall

Primary ::= NUMBER
         | IDENTIFIER
         | IDENTIFIER '[' EXPRESSION ']'  # Array access

BinOp ::= '+' | '-' | '*' | '/' | '%'
       | '==' | '!=' | '<' | '>' | '<=' | '>='
       | '&&' | '||'

UnaryOp ::= '-' | '!'

FunctionCall ::= IDENTIFIER '(' [EXPRESSION (',' EXPRESSION)*] ')'
```

### String Interpolation

Strings support two forms of interpolation:
- `${variable}` - Variable reference (can be string or numeric)
- `${expression}` - Numeric expression evaluation

Both work in:
- Element text content
- Element IDs (creates dynamic IDs like `cell${row}${col}`)
- Text update commands
- Variable declarations

### Scoping Rules

- **Top-level variables**: Visible in entire script including event handlers
- **Loop variables**: Created automatically as `int`, visible in loop body and nested loops
- **Global variables**: Marked with `global`, persist across presentation slides
- **Arrays**: Declared with size, all elements initialized to same value

### Execution Contexts

Revel DSL has two execution contexts with nearly identical capabilities:

#### 1. **Top-level context** (Main script)

**All DSL commands available:**
- Variable declarations (`int`, `real`, `bool`, `string`, arrays, `global`)
- Element creation (all types: notes, text, shapes, images, videos, audio, spaces)
- `connect` - Arrow connections
- `canvas_background` - Canvas configuration
- `animation_mode` [cycled] - Animation timeline mode
- `animation_next_slide` - Presentation slide markers
- `on click`/`on variable` - Event handler registration
- `set` - Variable assignment
- `animate_*` - Animation commands (**requires `animation_mode` first**)
- `text_update`, `text_bind`, `position_bind` - Text manipulation
- `presentation_next`, `presentation_auto_next_if` - Slide control
- `for` loops - Full DSL support including variable declarations

**Animation behavior:**
- Requires `animation_mode` declaration before using `animate_*`
- Creates declarative timeline that plays on script load

#### 2. **Event handler context** (`on click`, `on variable` blocks)

**Nearly all DSL commands available:**
- **Full DSL support** - Uses same script processor as top-level
- **Variable declarations** (`int`, `real`, `bool`, `string`, arrays) - **Can declare new variables inside handlers**
- Element creation (all types: notes, text, shapes, images, videos, audio, spaces)
- `connect` - Arrow connections
- `set` - Variable assignment (existing or new variables)
- `animate_*` - Animation commands (**works without `animation_mode`**)
- `text_update`, `text_bind`, `position_bind` - Text manipulation
- `presentation_next`, `presentation_auto_next_if` - Slide control
- `for` loops - Full DSL support including variable declarations and element creation

**Only 4 restrictions:**
- `canvas_background` - Must be set at top level (canvas is already initialized)
- `animation_mode` - Must be declared at top level (animation timeline is declarative)
- `animation_next_slide` - Presentation structure defined at top level
- `on click`/`on variable` - **Cannot nest event handlers**

**Animation behavior:**
- Works **without** `animation_mode` declaration
- Queues animations dynamically in response to events
- Animations execute when event handler completes

### Key Differences Summary

| Feature | Top-Level | Event Handler |
|---------|-----------|---------------|
| Variable declarations | Yes | Yes |
| Element creation (all types) | Yes | Yes |
| Connections | Yes | Yes |
| Animations | Yes - Requires `animation_mode` | Yes - No requirement |
| For loops | Yes - Full DSL | Yes - Full DSL |
| Canvas configuration | Yes | No - Top-level only |
| Event registration | Yes | No - Cannot nest |
| Presentation slides | Yes | No - Top-level only |

**Summary:** Event handlers support the full DSL except for meta-configuration (canvas settings, animation/presentation mode setup, nested event handlers).

---

## Tutorial: Rule 110 Cellular Automaton

Let's build something wild - a **Turing-complete** cellular automaton that can theoretically compute anything! We'll use `examples/rule110.dsl` to create a 500×500 simulator where simple rules produce complex, beautiful patterns.

### The Big Picture

Click START → 500 generations of black/white cells evolve based on Rule 110's pattern matching. Each generation is a row of pixels spawning the next generation below it.

### Part 1: Setup & Data Structures

```dsl
canvas_background (0.05,0.05,0.08,1.0) false

int c[500] 0        # Current generation: 500 cells (0=dead, 1=alive)
set c[499] 1        # Seed: rightmost cell starts alive

int next[500] 0     # Next generation buffer
int gen 0           # Generation counter
int step 0          # State machine: 0=idle, 1=draw, 2=compute, 3=copy
```

Two arrays do double-buffering: compute `next` from `c`, then swap. The `step` variable drives our three-phase loop.

### Part 2: UI & Interaction

```dsl
shape_create btn rectangle "START" (400,20) (150,50) filled true bg (0.2,0.6,0.9,1.0)
text_create lbl "Gen: 0 / 500" (570,30) (150,30) text_color (0.8,0.8,0.8,1.0)

on click btn
  set step 1        # Kick off the state machine!
end
```

One click launches the whole simulation. Notice how `on click` makes any element interactive.

### Part 3: The State Machine

**Phase 1 - Draw** (when `step == 1`)

```dsl
on variable step == 1
  set gen {gen + 1}
  for i 0 499
    shape_create r${gen}c${i} rectangle "" ({10+i*6},{120+gen*6}) (5,5) filled true bg {c[i],c[i],c[i],1.0}
  end
  text_update lbl "Gen: ${gen} / 500"
  set step 2
end
```

The magic: `r${gen}c${i}` creates unique IDs like `r1c0`, `r1c1`... dynamically! Color `{c[i],c[i],c[i],1.0}` maps 0→black, 1→white. Each generation is drawn 6 pixels below the last.

**Phase 2 - Compute** (when `step == 2`)

Rule 110's lookup table says: if you see patterns 1,2,3,5,6 in binary (like `010` or `110`), the center cell lives. Otherwise it dies.

```dsl
on variable step == 2
  # Convert 3 neighbors to binary: left*4 + center*2 + right
  set pattern {left * 4 + c[i] * 2 + right}

  # Check if pattern is 1,2,3,5, or 6 (boolean math trick!)
  set next[i] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}
end
```

The boolean trick: each comparison is 0 or 1, summing them gives 1 if any match! No if-statements needed. The code handles edges (cells 0 and 499) by assuming dead neighbors outside bounds.

**Phase 3 - Copy & Loop** (when `step == 3`)

```dsl
on variable step == 3
  for i 0 499
    set c[i] {next[i]}          # Swap buffers
  end
  set step {(gen < 500) * 1}    # Continue or stop
end
```

Clever continuation: `{(gen < 500) * 1}` evaluates to 1 (repeat) or 0 (done). When `step` becomes 1 again, the whole cycle repeats!

### The Flow

```
Click START → step=1 → Draw → step=2 → Compute → step=3 → Copy → step=1 (or 0 if done)
```

Each phase triggers the next by changing `step`, and `on variable step == N` handlers react instantly. It's event-driven programming!

### Cool Tricks You Just Learned

- **Dynamic IDs**: `r${gen}c${i}` creates thousands of unique element names
- **Boolean arithmetic**: `{(pattern == 1) + (pattern == 2) + ...}` = no if-statements needed
- **Expression syntax**: `{expr}` for math, `${var}` for text/IDs
- **State machines**: One variable (`step`) orchestrates complex behavior
- **Double buffering**: Two arrays prevent read-while-writing bugs
- **Event chains**: Variable changes trigger handlers which trigger more handlers

### Try It

```bash
./revel --dsl examples/rule110.dsl
```

Click START and watch Turing completeness unfold!

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

### Control Flow

```
for VARIABLE START END
  # commands
end
```

- Loops from START to END (inclusive)
- VARIABLE is automatically created as `int` if not declared
- START and END can be literals, variables, or expressions
- Can be nested (for within for)
- Works at **top level** (main script) AND inside event blocks (`on click`, `on variable`)
- **Top-level loops** support ALL DSL commands: variable declarations, element creation, connections, nested loops, etc.
- **Event block loops** support runtime commands: `set`, `animate_*`, `text_update`, element creation, nested loops

**Example:**
```
# Top-level loops - full DSL support
for i 0 59
  set cells[i] 0
end

# Nested loops with variable declarations and dynamic IDs
for row 0 9
  for col 0 9
    int color {(row + col) % 3}
    shape_create cell${row}${col} rectangle "" ({col*50},{row*50}) (48,48) filled true bg {color,color,color,1.0}
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
