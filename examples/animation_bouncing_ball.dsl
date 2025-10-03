# Bouncing Ball at Angle - Physics Simulation
# Ball bounces diagonally with decreasing height (energy loss)

animation_mode cycled

# Create a bouncing ball (larger for visibility)
shape_create ball circle "⚽" (70,70) (100,100) bg=(1.0,0.8,0.2,1.0) filled true stroke 4 stroke_color=(0.8,0.6,0.0,1.0) font "Ubuntu 32" text_color=(0.2,0.2,0.2,1.0)

# Create ground
note_create ground "Ground" (50,450) (700,60) bg=(0.4,0.3,0.2,1.0) font "Ubuntu Bold 24" text_color=(1.0,1.0,1.0,1.0) align=center

# Bounce sequence with decreasing height (energy loss simulation)
# Ground is at y=450, ball is 100px tall, so ground contact is at y=350 (450-100)

# Drop and first bounce (highest)
animate_move ball (70,70) (200,350) 0.0 0.8 bezier
animate_move ball (200,350) (300,150) 0.8 0.6 bezier

# Second bounce: lower height
animate_move ball (300,150) (400,350) 1.4 0.5 bezier
animate_move ball (400,350) (500,220) 1.9 0.5 bezier

# Third bounce: even lower
animate_move ball (500,220) (600,350) 2.4 0.4 bezier
animate_move ball (600,350) (650,270) 2.8 0.3 bezier

# Small bounces as energy dissipates
animate_move ball (650,270) (680,350) 3.1 0.25 bezier
animate_move ball (680,350) (700,310) 3.35 0.2 bezier

# Final small bounce and settle on ground
animate_move ball (700,310) (710,350) 3.55 0.15 linear

# Roll back to start along the ground
animate_move ball (710,350) (70,350) 3.7 1.2 linear

# Pop back up to starting position
animate_move ball (70,350) (70,70) 4.9 0.3 bezier
