# Plot/Graph Demo - showcasing line graph rendering from CSV data
# Plots auto-scale with axes starting from 0, gridlines show actual values

# Plot 1: Multi-line plot with legend
# Shows multiple data series with automatic color assignment
# Note: In DSL, use line names without quotes (or spaces)
shape_create plot1 plot "line Temperature 0,10 1,25 2,20 3,35 4,30 5,45 6,50 7,42 8,60 9,55\nline Humidity 0,15 1,22 2,18 3,30 4,28 5,35 6,40 7,38 8,48 9,45" (100, 100) (500, 350) stroke_width 2

# Plot 2: Three-line comparison
# Legend appears in top-right corner
shape_create plot2 plot "line ProductA 0,5 2,15 4,12 6,25 8,20 10,30\nline ProductB 0,8 2,18 4,16 6,28 8,24 10,35\nline ProductC 0,3 2,12 4,9 6,20 8,16 10,25" (650, 100) (500, 350) stroke_width 2

# Plot 3: Y values only (X auto-indexed from 0) - single line
# Grid shows actual Y values: 0, 12, 24, 36, 48, 60
shape_create plot3 plot "10\n25\n15\n35\n20\n45\n30\n50\n40\n55" (100, 500) (400, 300) stroke_color #22c55e stroke_width 3

# Plot 4: Higher range data - axes still start at 0
# This shows the full scale from 0 to max, so you see relative magnitude
shape_create plot4 plot "0, 150\n2, 220\n4, 180\n6, 280\n8, 250\n10, 320" (550, 500) (400, 300) stroke_color #f59e0b stroke_width 2
