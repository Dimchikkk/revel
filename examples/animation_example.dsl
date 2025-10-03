# Simple Animation Example - Moving Circle
# This demonstrates the 3 animation types: immediate, linear, and bezier

animation_mode

# Create three circles
note_create circle1 "Immediate" (100,100) (150,50) bg=(0.8,0.2,0.2,1.0)
note_create circle2 "Linear" (100,200) (150,50) bg=(0.2,0.8,0.2,1.0)
note_create circle3 "Bezier" (100,300) (150,50) bg=(0.2,0.2,0.8,1.0)

# Animate them moving from left to right with different interpolations
# animate_move ELEMENT_ID (from_x,from_y) (to_x,to_y) START_TIME DURATION [TYPE]

# Immediate: jumps instantly at 0.5 seconds
animate_move circle1 (100,100) (600,100) 0.5 0.0 immediate

# Linear: constant speed over 2 seconds
animate_move circle2 (100,200) (600,200) 0.0 2.0 linear

# Bezier: smooth ease-in-out over 2 seconds
animate_move circle3 (100,300) (600,300) 0.0 2.0 bezier
