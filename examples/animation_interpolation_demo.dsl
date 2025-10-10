# Animation Interpolation Types - Recording Demo
# Slide 1: Press START button, Slide 2: Auto-plays animation

global int ready 0

# ============================================================================
# Slide 1: Title and START button
# ============================================================================

canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)

text_create demo_title "Animation Interpolation Types" (400,200) (600,80) text_color #60a5fa font "Ubuntu Bold 48" align center
text_create demo_subtitle "8 Different Easing Functions" (400,300) (600,50) text_color #94a3b8 font "Ubuntu 28" align center

# START button
shape_create start_btn roundedrect "START" (400,450) (300,100) bg #10b981 text_color #ffffff font "Ubuntu Bold 36" filled true

text_create instruction "Click START to begin the demo" (400,600) (500,40) text_color #f8fafc font "Ubuntu 18" align center

on click start_btn
  set ready {ready + 1}
end

on variable ready
  presentation_auto_next_if ready 1
end

animation_next_slide

# ============================================================================
# Slide 2: Animation Demo - Auto-plays when slide loads
# ============================================================================

animation_mode

canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)

# Create 8 circles, one for each interpolation type
shape_create circle1 circle "" (200,80) (40,40) bg #ff0000 filled true
shape_create circle2 circle "" (200,160) (40,40) bg #ff7700 filled true
shape_create circle3 circle "" (200,240) (40,40) bg #ffff00 filled true
shape_create circle4 circle "" (200,320) (40,40) bg #00ff00 filled true
shape_create circle5 circle "" (200,400) (40,40) bg #00ffff filled true
shape_create circle6 circle "" (200,480) (40,40) bg #0077ff filled true
shape_create circle7 circle "" (200,560) (40,40) bg #7700ff filled true
shape_create circle8 circle "" (200,640) (40,40) bg #ff00ff filled true

# Add labels to identify each type - positioned to the left of circles
text_create label1 "immediate" (20,70) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label2 "linear" (20,150) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label3 "bezier" (20,230) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label4 "ease-in" (20,310) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label5 "ease-out" (20,390) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label6 "elastic" (20,470) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label7 "back" (20,550) (160,30) text_color #ffffff font "Ubuntu 16"
text_create label8 "bounce" (20,630) (160,30) text_color #ffffff font "Ubuntu 16"

# PHASE 1: Movement animations (0-2.5s)
# All move right with their respective interpolation type

animate_move circle1 (200,80) (900,80) 0 2.5 immediate
animate_move circle2 (200,160) (900,160) 0 2.5 linear
animate_move circle3 (200,240) (900,240) 0 2.5 bezier
animate_move circle4 (200,320) (900,320) 0 2.5 ease-in
animate_move circle5 (200,400) (900,400) 0 2.5 ease-out
animate_move circle6 (200,480) (900,480) 0 2.5 elastic
animate_move circle7 (200,560) (900,560) 0 2.5 back
animate_move circle8 (200,640) (900,640) 0 2.5 bounce

# PHASE 2: Size animations while moving back (3-5s)
# Grow with their interpolation type, then shrink back

animate_move circle1 (900,80) (200,80) 3 2 immediate
animate_move circle2 (900,160) (200,160) 3 2 linear
animate_move circle3 (900,240) (200,240) 3 2 bezier
animate_move circle4 (900,320) (200,320) 3 2 ease-in
animate_move circle5 (900,400) (200,400) 3 2 ease-out
animate_move circle6 (900,480) (200,480) 3 2 elastic
animate_move circle7 (900,560) (200,560) 3 2 back
animate_move circle8 (900,640) (200,640) 3 2 bounce

# Grow while moving back
animate_resize circle1 (40,40) (80,80) 3 2 immediate
animate_resize circle2 (40,40) (80,80) 3 2 linear
animate_resize circle3 (40,40) (80,80) 3 2 bezier
animate_resize circle4 (40,40) (80,80) 3 2 ease-in
animate_resize circle5 (40,40) (80,80) 3 2 ease-out
animate_resize circle6 (40,40) (80,80) 3 2 elastic
animate_resize circle7 (40,40) (80,80) 3 2 back
animate_resize circle8 (40,40) (80,80) 3 2 bounce

# PHASE 3: Color changes (5.5-7s)
animate_color circle1 #ff0000 #00ff00 5.5 1.5 immediate
animate_color circle2 #ff7700 #00ff00 5.5 1.5 linear
animate_color circle3 #ffff00 #00ff00 5.5 1.5 bezier
animate_color circle4 #00ff00 #0000ff 5.5 1.5 ease-in
animate_color circle5 #00ffff #0000ff 5.5 1.5 ease-out
animate_color circle6 #0077ff #ff00ff 5.5 1.5 elastic
animate_color circle7 #7700ff #ff0000 5.5 1.5 back
animate_color circle8 #ff00ff #ffff00 5.5 1.5 bounce

# PHASE 4: Shrink back (7.5-9s)
animate_resize circle1 (80,80) (40,40) 7.5 1.5 immediate
animate_resize circle2 (80,80) (40,40) 7.5 1.5 linear
animate_resize circle3 (80,80) (40,40) 7.5 1.5 bezier
animate_resize circle4 (80,80) (40,40) 7.5 1.5 ease-in
animate_resize circle5 (80,80) (40,40) 7.5 1.5 ease-out
animate_resize circle6 (80,80) (40,40) 7.5 1.5 elastic
animate_resize circle7 (80,80) (40,40) 7.5 1.5 back
animate_resize circle8 (80,80) (40,40) 7.5 1.5 bounce
