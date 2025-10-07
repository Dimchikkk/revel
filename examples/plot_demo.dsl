# Plot/Graph Demo - showcasing line graph rendering from CSV data
# Plots auto-scale with axes starting from 0, gridlines show actual values

# Plot 1: X,Y pairs from 0-9, showing growth trend
# Y-axis will show 0 to ~66 with labeled gridlines
shape_create plot1 plot "0, 10\n1, 25\n2, 20\n3, 35\n4, 30\n5, 45\n6, 50\n7, 42\n8, 60\n9, 55" (100, 100) (400, 300) stroke_color #3b82f6 stroke_width 2

# Plot 2: Sparser data (even X values only)
# Axes start at 0,0 and extend to cover max values with 10% padding
shape_create plot2 plot "0, 5\n2, 15\n4, 12\n6, 25\n8, 20\n10, 30" (550, 100) (400, 300) stroke_color #ef4444 stroke_width 2

# Plot 3: Y values only (X auto-indexed from 0)
# Grid shows actual Y values: 0, 12, 24, 36, 48, 60
shape_create plot3 plot "10\n25\n15\n35\n20\n45\n30\n50\n40\n55" (100, 450) (400, 300) stroke_color #22c55e stroke_width 3

# Plot 4: Higher range data - axes still start at 0
# This shows the full scale from 0 to max, so you see relative magnitude
shape_create plot4 plot "0, 150\n2, 220\n4, 180\n6, 280\n8, 250\n10, 320" (550, 450) (400, 300) stroke_color #f59e0b stroke_width 2
