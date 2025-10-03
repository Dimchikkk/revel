# Elegant Animation Demo
# Demonstrates all animation types: move, resize, color, appear, disappear
# With different interpolation methods: immediate, linear, bezier

animation_mode

# Dark elegant background
canvas_background (0.08,0.08,0.12,1.0) false

# ============================================================================
# Title sequence - Fade in and slide
# ============================================================================

note_create title "Revel Animations" (350,180) (500,60) bg #1e293b text_color #60a5fa font "Ubuntu Bold 28"
animate_appear title 0.0 0.8 bezier
animate_move title (350,130) (350,180) 0.0 1.0 bezier

text_create subtitle "Smooth • Beautiful • Simple" (350,255) (500,30) text_color #94a3b8 font "Ubuntu 16"
animate_appear subtitle 0.8 0.6 bezier

# ============================================================================
# Movement showcase - Dots forming "REVEL"
# ============================================================================

# R - Letter (dots start at center, move to form R)
shape_create r1 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r2 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r3 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r4 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r5 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r6 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r7 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r8 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r9 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r10 circle "" (600,450) (18,18) bg #ef4444 filled true
shape_create r11 circle "" (600,450) (18,18) bg #ef4444 filled true

# E - Letter
shape_create e1 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e2 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e3 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e4 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e5 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e6 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e7 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e8 circle "" (600,450) (18,18) bg #f59e0b filled true
shape_create e9 circle "" (600,450) (18,18) bg #f59e0b filled true

# V - Letter
shape_create v1 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v2 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v3 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v4 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v5 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v6 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v7 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v8 circle "" (600,450) (18,18) bg #10b981 filled true
shape_create v9 circle "" (600,450) (18,18) bg #10b981 filled true

# E2 - Letter
shape_create e2_1 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_2 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_3 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_4 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_5 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_6 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_7 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_8 circle "" (600,450) (18,18) bg #3b82f6 filled true
shape_create e2_9 circle "" (600,450) (18,18) bg #3b82f6 filled true

# L - Letter
shape_create l1 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l2 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l3 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l4 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l5 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l6 circle "" (600,450) (18,18) bg #8b5cf6 filled true
shape_create l7 circle "" (600,450) (18,18) bg #8b5cf6 filled true

# Appear all dots
animate_appear r1 2.0 0.3 bezier
animate_appear r2 2.0 0.3 bezier
animate_appear r3 2.0 0.3 bezier
animate_appear r4 2.0 0.3 bezier
animate_appear r5 2.0 0.3 bezier
animate_appear r6 2.0 0.3 bezier
animate_appear r7 2.0 0.3 bezier
animate_appear r8 2.0 0.3 bezier
animate_appear r9 2.0 0.3 bezier
animate_appear r10 2.0 0.3 bezier
animate_appear r11 2.0 0.3 bezier
animate_appear e1 2.0 0.3 bezier
animate_appear e2 2.0 0.3 bezier
animate_appear e3 2.0 0.3 bezier
animate_appear e4 2.0 0.3 bezier
animate_appear e5 2.0 0.3 bezier
animate_appear e6 2.0 0.3 bezier
animate_appear e7 2.0 0.3 bezier
animate_appear e8 2.0 0.3 bezier
animate_appear e9 2.0 0.3 bezier
animate_appear v1 2.0 0.3 bezier
animate_appear v2 2.0 0.3 bezier
animate_appear v3 2.0 0.3 bezier
animate_appear v4 2.0 0.3 bezier
animate_appear v5 2.0 0.3 bezier
animate_appear v6 2.0 0.3 bezier
animate_appear v7 2.0 0.3 bezier
animate_appear v8 2.0 0.3 bezier
animate_appear v9 2.0 0.3 bezier
animate_appear e2_1 2.0 0.3 bezier
animate_appear e2_2 2.0 0.3 bezier
animate_appear e2_3 2.0 0.3 bezier
animate_appear e2_4 2.0 0.3 bezier
animate_appear e2_5 2.0 0.3 bezier
animate_appear e2_6 2.0 0.3 bezier
animate_appear e2_7 2.0 0.3 bezier
animate_appear e2_8 2.0 0.3 bezier
animate_appear e2_9 2.0 0.3 bezier
animate_appear l1 2.0 0.3 bezier
animate_appear l2 2.0 0.3 bezier
animate_appear l3 2.0 0.3 bezier
animate_appear l4 2.0 0.3 bezier
animate_appear l5 2.0 0.3 bezier
animate_appear l6 2.0 0.3 bezier
animate_appear l7 2.0 0.3 bezier

# Move dots to form "REVEL" - R at x=160
# R shape: vertical line (5) + top curve (3) + middle (1) + diagonal leg (2)
animate_move r1 (600,450) (160,380) 2.5 1.8 bezier
animate_move r2 (600,450) (160,410) 2.5 1.8 bezier
animate_move r3 (600,450) (160,440) 2.5 1.8 bezier
animate_move r4 (600,450) (160,470) 2.5 1.8 bezier
animate_move r5 (600,450) (160,500) 2.5 1.8 bezier
animate_move r6 (600,450) (180,380) 2.5 1.8 bezier
animate_move r7 (600,450) (190,380) 2.5 1.8 bezier
animate_move r8 (600,450) (190,410) 2.5 1.8 bezier
animate_move r9 (600,450) (180,440) 2.5 1.8 bezier
animate_move r10 (600,450) (190,470) 2.5 1.8 bezier
animate_move r11 (600,450) (200,500) 2.5 1.8 bezier

# E at x=260
# E shape: vertical line (5) + top bar (2) + middle bar (2) + bottom bar (2)
animate_move e1 (600,450) (260,380) 2.5 1.8 bezier
animate_move e2 (600,450) (260,410) 2.5 1.8 bezier
animate_move e3 (600,450) (260,440) 2.5 1.8 bezier
animate_move e4 (600,450) (260,470) 2.5 1.8 bezier
animate_move e5 (600,450) (260,500) 2.5 1.8 bezier
animate_move e6 (600,450) (280,380) 2.5 1.8 bezier
animate_move e7 (600,450) (290,380) 2.5 1.8 bezier
animate_move e8 (600,450) (280,440) 2.5 1.8 bezier
animate_move e9 (600,450) (290,500) 2.5 1.8 bezier

# V at x=360
# V shape: left side (4) + bottom point (1) + right side (4)
animate_move v1 (600,450) (360,380) 2.5 1.8 bezier
animate_move v2 (600,450) (365,410) 2.5 1.8 bezier
animate_move v3 (600,450) (370,440) 2.5 1.8 bezier
animate_move v4 (600,450) (375,470) 2.5 1.8 bezier
animate_move v5 (600,450) (380,500) 2.5 1.8 bezier
animate_move v6 (600,450) (385,470) 2.5 1.8 bezier
animate_move v7 (600,450) (390,440) 2.5 1.8 bezier
animate_move v8 (600,450) (395,410) 2.5 1.8 bezier
animate_move v9 (600,450) (400,380) 2.5 1.8 bezier

# E2 at x=460
# E shape: vertical line (5) + top bar (2) + middle bar (2) + bottom bar (2)
animate_move e2_1 (600,450) (460,380) 2.5 1.8 bezier
animate_move e2_2 (600,450) (460,410) 2.5 1.8 bezier
animate_move e2_3 (600,450) (460,440) 2.5 1.8 bezier
animate_move e2_4 (600,450) (460,470) 2.5 1.8 bezier
animate_move e2_5 (600,450) (460,500) 2.5 1.8 bezier
animate_move e2_6 (600,450) (480,380) 2.5 1.8 bezier
animate_move e2_7 (600,450) (490,380) 2.5 1.8 bezier
animate_move e2_8 (600,450) (480,440) 2.5 1.8 bezier
animate_move e2_9 (600,450) (490,500) 2.5 1.8 bezier

# L at x=560
# L shape: vertical line (5) + bottom horizontal (2)
animate_move l1 (600,450) (560,380) 2.5 1.8 bezier
animate_move l2 (600,450) (560,410) 2.5 1.8 bezier
animate_move l3 (600,450) (560,440) 2.5 1.8 bezier
animate_move l4 (600,450) (560,470) 2.5 1.8 bezier
animate_move l5 (600,450) (560,500) 2.5 1.8 bezier
animate_move l6 (600,450) (580,500) 2.5 1.8 bezier
animate_move l7 (600,450) (590,500) 2.5 1.8 bezier

# ============================================================================
# Grow and shrink
# ============================================================================

shape_create pulse roundedrect "Pulse" (200,720) (70,70) bg #8b5cf6 filled true text_color #ffffff font "Ubuntu Bold 11"
animate_appear pulse 5.0 0.3 bezier
animate_resize pulse (70,70) (130,130) 5.5 1.0 bezier
animate_resize pulse (130,130) (85,85) 6.5 0.8 bezier

# ============================================================================
# Color morphing
# ============================================================================

note_create morph "Color" (500,720) (140,90) bg #dc2626 text_color #ffffff font "Ubuntu Bold 14"
animate_appear morph 5.0 0.3 bezier
animate_color morph #dc2626 #7c3aed 5.5 2.0 bezier
animate_color morph #7c3aed #0891b2 7.5 1.5 bezier

# ============================================================================
# Combined effect - The magic box
# ============================================================================

shape_create magic roundedrect "✨" (820,720) (90,90) bg #f59e0b filled true text_color #ffffff font "Ubuntu Bold 32"
animate_appear magic 9.0 0.5 bezier
animate_move magic (820,720) (820,540) 9.5 1.5 bezier
animate_resize magic (90,90) (160,160) 9.5 1.5 bezier
animate_color magic #f59e0b #ec4899 9.5 1.5 bezier

# ============================================================================
# Finale - Gentle fade out
# ============================================================================

animate_disappear title 10.5 2.0 bezier
animate_disappear subtitle 10.5 2.0 bezier
animate_disappear r1 10.6 1.8 linear
animate_disappear r2 10.6 1.8 linear
animate_disappear r3 10.6 1.8 linear
animate_disappear r4 10.6 1.8 linear
animate_disappear r5 10.6 1.8 linear
animate_disappear r6 10.6 1.8 linear
animate_disappear r7 10.6 1.8 linear
animate_disappear r8 10.6 1.8 linear
animate_disappear r9 10.6 1.8 linear
animate_disappear r10 10.6 1.8 linear
animate_disappear r11 10.6 1.8 linear
animate_disappear e1 10.7 1.8 linear
animate_disappear e2 10.7 1.8 linear
animate_disappear e3 10.7 1.8 linear
animate_disappear e4 10.7 1.8 linear
animate_disappear e5 10.7 1.8 linear
animate_disappear e6 10.7 1.8 linear
animate_disappear e7 10.7 1.8 linear
animate_disappear e8 10.7 1.8 linear
animate_disappear e9 10.7 1.8 linear
animate_disappear v1 10.8 1.8 linear
animate_disappear v2 10.8 1.8 linear
animate_disappear v3 10.8 1.8 linear
animate_disappear v4 10.8 1.8 linear
animate_disappear v5 10.8 1.8 linear
animate_disappear v6 10.8 1.8 linear
animate_disappear v7 10.8 1.8 linear
animate_disappear v8 10.8 1.8 linear
animate_disappear v9 10.8 1.8 linear
animate_disappear e2_1 10.9 1.8 linear
animate_disappear e2_2 10.9 1.8 linear
animate_disappear e2_3 10.9 1.8 linear
animate_disappear e2_4 10.9 1.8 linear
animate_disappear e2_5 10.9 1.8 linear
animate_disappear e2_6 10.9 1.8 linear
animate_disappear e2_7 10.9 1.8 linear
animate_disappear e2_8 10.9 1.8 linear
animate_disappear e2_9 10.9 1.8 linear
animate_disappear l1 11.0 1.8 linear
animate_disappear l2 11.0 1.8 linear
animate_disappear l3 11.0 1.8 linear
animate_disappear l4 11.0 1.8 linear
animate_disappear l5 11.0 1.8 linear
animate_disappear l6 11.0 1.8 linear
animate_disappear l7 11.0 1.8 linear
animate_disappear pulse 10.5 2.0 linear
animate_disappear morph 10.5 2.0 linear
animate_disappear magic 10.5 2.0 bezier

# Invisible marker to hold the animation open for 1 more second after everything fades
shape_create end_marker circle "" (0,0) (1,1) bg (0,0,0,0) filled true
animate_appear end_marker 13.5 0.0 immediate
