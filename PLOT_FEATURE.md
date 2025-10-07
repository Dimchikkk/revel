# Plot/Graph Shape Feature

## Overview
New shape type `SHAPE_PLOT` that renders line graphs from CSV-like data.

## Features

### Data Input Formats
The plot shape accepts text data in the following formats:

1. **X,Y pairs** (comma or space-separated):
   ```
   0, 10
   1, 25
   2, 20
   3, 35
   ```

2. **Single Y values** (uses index as X):
   ```
   10
   25
   20
   35
   ```

### Visual Elements
- Automatic axis rendering with labeled gridlines (X and Y)
- **Axes always start from 0** - ensures visual representation shows true scale
- Grid lines with value labels (5 divisions on each axis)
- Connected line plot with configurable stroke width
- Circular markers at each data point
- Labeled margins (50px left, 30px bottom, 20px top/right) for axis labels

**Example**: Data point (0, 300) will be shown at the origin with Y-axis going up to ~330 (with 10% padding). The gridlines will show values like 0, 66, 132, 198, 264, 330 so you can see the actual scale.

## Usage

### 1. Shape Designer Dialog
1. Click the shape designer button (or use keyboard shortcut)
2. Press `G` or click "Plot" button
3. Edit the text to add your data points

### 2. DSL (Domain Specific Language)
```dsl
# Basic plot with X,Y pairs (use \n for newlines)
shape_create myplot plot "0, 10\n1, 25\n2, 20\n3, 35" (100, 100) (400, 300) stroke_color #3b82f6 stroke_width 2

# Plot with single Y values (auto-indexed)
shape_create values plot "10\n25\n20\n35" (100, 100) (400, 300) stroke_color #ef4444 stroke_width 3
```

**Note**: Use `\n` to separate data points in DSL (actual newlines are not supported in DSL string literals)

### Supported DSL Keywords
- `plot`
- `graph` (alias)

## Implementation Files
- `src/shape.h` - Added `SHAPE_PLOT` enum
- `src/shape.c` - Plot rendering logic (lines 626-763)
- `src/shape_dialog.c` - UI integration
- `src/dsl/dsl_utils.c` - DSL parser support

## Styling Options
- `stroke_color` - Line and point color
- `stroke_width` - Line thickness (also affects point size)
- Standard shape options (bg_color, rotation, etc.)

## Example Demo File
See `examples/plot_demo.dsl` for working examples with 4 different plots demonstrating various data ranges and scaling behavior.

Run it with:
```bash
rm -f demo.db && ./revel --dsl examples/plot_demo.dsl demo.db
```
