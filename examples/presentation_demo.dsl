# Simple Presentation Demo
# Use Ctrl+Right Arrow to advance, Ctrl+Left Arrow to go back

# Slide 1: Title Slide with fade-in animation
animation_mode
canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)
text_create title "Revel Presentations" (300,250) (600,120) text_color #60a5fa font "Ubuntu Bold 56" align center
text_create subtitle "Interactive Slides from DSL" (300,400) (600,60) text_color #94a3b8 font "Ubuntu 28" align center
animate_appear title 0.0 1.2 bezier
animate_appear subtitle 1.0 1.0 bezier

animation_next_slide

# Slide 2: Features with animation
animation_mode
canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)
text_create header "What Can You Do?" (100,80) (800,80) text_color #3b82f6 font "Ubuntu Bold 42"
shape_create box1 roundedrect "✨ Create Elements" (120,200) (350,100) bg #1e40af text_color #FFFFFF font "Ubuntu Bold 20" filled true
shape_create box2 roundedrect "🎨 Add Shapes & Media" (520,200) (350,100) bg #7c3aed text_color #FFFFFF font "Ubuntu Bold 20" filled true
shape_create box3 roundedrect "🔗 Make Connections" (120,340) (350,100) bg #059669 text_color #FFFFFF font "Ubuntu Bold 20" filled true
shape_create box4 roundedrect "📊 Build Diagrams" (520,340) (350,100) bg #dc2626 text_color #FFFFFF font "Ubuntu Bold 20" filled true
animate_appear header 0.0 0.8 bezier
animate_appear box1 0.5 0.6 bezier
animate_appear box2 0.8 0.6 bezier
animate_appear box3 1.1 0.6 bezier
animate_appear box4 1.4 0.6 bezier

animation_next_slide

# Slide 3: REVEL word formation animation (cycled)
animation_mode cycled
canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)
text_create word_title "Animated Logo" (300,80) (400,60) text_color #60a5fa font "Ubuntu Bold 32"

# R - Letter (dots start at center, move to form R)
shape_create r1 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r2 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r3 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r4 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r5 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r6 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r7 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r8 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r9 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r10 circle "" (500,300) (15,15) bg #ef4444 filled true
shape_create r11 circle "" (500,300) (15,15) bg #ef4444 filled true

# E - Letter
shape_create e1 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e2 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e3 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e4 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e5 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e6 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e7 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e8 circle "" (500,300) (15,15) bg #f59e0b filled true
shape_create e9 circle "" (500,300) (15,15) bg #f59e0b filled true

# V - Letter
shape_create v1 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v2 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v3 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v4 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v5 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v6 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v7 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v8 circle "" (500,300) (15,15) bg #10b981 filled true
shape_create v9 circle "" (500,300) (15,15) bg #10b981 filled true

# E2 - Letter
shape_create e2_1 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_2 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_3 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_4 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_5 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_6 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_7 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_8 circle "" (500,300) (15,15) bg #3b82f6 filled true
shape_create e2_9 circle "" (500,300) (15,15) bg #3b82f6 filled true

# L - Letter
shape_create l1 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l2 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l3 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l4 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l5 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l6 circle "" (500,300) (15,15) bg #8b5cf6 filled true
shape_create l7 circle "" (500,300) (15,15) bg #8b5cf6 filled true

# Move dots to form "REVEL" - R at x=160
animate_move r1 (500,300) (160,250) 0.0 1.5 bezier
animate_move r2 (500,300) (160,280) 0.0 1.5 bezier
animate_move r3 (500,300) (160,310) 0.0 1.5 bezier
animate_move r4 (500,300) (160,340) 0.0 1.5 bezier
animate_move r5 (500,300) (160,370) 0.0 1.5 bezier
animate_move r6 (500,300) (180,250) 0.0 1.5 bezier
animate_move r7 (500,300) (190,250) 0.0 1.5 bezier
animate_move r8 (500,300) (190,280) 0.0 1.5 bezier
animate_move r9 (500,300) (180,310) 0.0 1.5 bezier
animate_move r10 (500,300) (190,340) 0.0 1.5 bezier
animate_move r11 (500,300) (200,370) 0.0 1.5 bezier

# E at x=260
animate_move e1 (500,300) (260,250) 0.0 1.5 bezier
animate_move e2 (500,300) (260,280) 0.0 1.5 bezier
animate_move e3 (500,300) (260,310) 0.0 1.5 bezier
animate_move e4 (500,300) (260,340) 0.0 1.5 bezier
animate_move e5 (500,300) (260,370) 0.0 1.5 bezier
animate_move e6 (500,300) (280,250) 0.0 1.5 bezier
animate_move e7 (500,300) (290,250) 0.0 1.5 bezier
animate_move e8 (500,300) (280,310) 0.0 1.5 bezier
animate_move e9 (500,300) (290,370) 0.0 1.5 bezier

# V at x=360
animate_move v1 (500,300) (360,250) 0.0 1.5 bezier
animate_move v2 (500,300) (365,280) 0.0 1.5 bezier
animate_move v3 (500,300) (370,310) 0.0 1.5 bezier
animate_move v4 (500,300) (375,340) 0.0 1.5 bezier
animate_move v5 (500,300) (380,370) 0.0 1.5 bezier
animate_move v6 (500,300) (385,340) 0.0 1.5 bezier
animate_move v7 (500,300) (390,310) 0.0 1.5 bezier
animate_move v8 (500,300) (395,280) 0.0 1.5 bezier
animate_move v9 (500,300) (400,250) 0.0 1.5 bezier

# E2 at x=460
animate_move e2_1 (500,300) (460,250) 0.0 1.5 bezier
animate_move e2_2 (500,300) (460,280) 0.0 1.5 bezier
animate_move e2_3 (500,300) (460,310) 0.0 1.5 bezier
animate_move e2_4 (500,300) (460,340) 0.0 1.5 bezier
animate_move e2_5 (500,300) (460,370) 0.0 1.5 bezier
animate_move e2_6 (500,300) (480,250) 0.0 1.5 bezier
animate_move e2_7 (500,300) (490,250) 0.0 1.5 bezier
animate_move e2_8 (500,300) (480,310) 0.0 1.5 bezier
animate_move e2_9 (500,300) (490,370) 0.0 1.5 bezier

# L at x=560
animate_move l1 (500,300) (560,250) 0.0 1.5 bezier
animate_move l2 (500,300) (560,280) 0.0 1.5 bezier
animate_move l3 (500,300) (560,310) 0.0 1.5 bezier
animate_move l4 (500,300) (560,340) 0.0 1.5 bezier
animate_move l5 (500,300) (560,370) 0.0 1.5 bezier
animate_move l6 (500,300) (580,370) 0.0 1.5 bezier
animate_move l7 (500,300) (590,370) 0.0 1.5 bezier

# Move back to center (loop)
animate_move r1 (160,250) (500,300) 1.5 1.5 bezier
animate_move r2 (160,280) (500,300) 1.5 1.5 bezier
animate_move r3 (160,310) (500,300) 1.5 1.5 bezier
animate_move r4 (160,340) (500,300) 1.5 1.5 bezier
animate_move r5 (160,370) (500,300) 1.5 1.5 bezier
animate_move r6 (180,250) (500,300) 1.5 1.5 bezier
animate_move r7 (190,250) (500,300) 1.5 1.5 bezier
animate_move r8 (190,280) (500,300) 1.5 1.5 bezier
animate_move r9 (180,310) (500,300) 1.5 1.5 bezier
animate_move r10 (190,340) (500,300) 1.5 1.5 bezier
animate_move r11 (200,370) (500,300) 1.5 1.5 bezier
animate_move e1 (260,250) (500,300) 1.5 1.5 bezier
animate_move e2 (260,280) (500,300) 1.5 1.5 bezier
animate_move e3 (260,310) (500,300) 1.5 1.5 bezier
animate_move e4 (260,340) (500,300) 1.5 1.5 bezier
animate_move e5 (260,370) (500,300) 1.5 1.5 bezier
animate_move e6 (280,250) (500,300) 1.5 1.5 bezier
animate_move e7 (290,250) (500,300) 1.5 1.5 bezier
animate_move e8 (280,310) (500,300) 1.5 1.5 bezier
animate_move e9 (290,370) (500,300) 1.5 1.5 bezier
animate_move v1 (360,250) (500,300) 1.5 1.5 bezier
animate_move v2 (365,280) (500,300) 1.5 1.5 bezier
animate_move v3 (370,310) (500,300) 1.5 1.5 bezier
animate_move v4 (375,340) (500,300) 1.5 1.5 bezier
animate_move v5 (380,370) (500,300) 1.5 1.5 bezier
animate_move v6 (385,340) (500,300) 1.5 1.5 bezier
animate_move v7 (390,310) (500,300) 1.5 1.5 bezier
animate_move v8 (395,280) (500,300) 1.5 1.5 bezier
animate_move v9 (400,250) (500,300) 1.5 1.5 bezier
animate_move e2_1 (460,250) (500,300) 1.5 1.5 bezier
animate_move e2_2 (460,280) (500,300) 1.5 1.5 bezier
animate_move e2_3 (460,310) (500,300) 1.5 1.5 bezier
animate_move e2_4 (460,340) (500,300) 1.5 1.5 bezier
animate_move e2_5 (460,370) (500,300) 1.5 1.5 bezier
animate_move e2_6 (480,250) (500,300) 1.5 1.5 bezier
animate_move e2_7 (490,250) (500,300) 1.5 1.5 bezier
animate_move e2_8 (480,310) (500,300) 1.5 1.5 bezier
animate_move e2_9 (490,370) (500,300) 1.5 1.5 bezier
animate_move l1 (560,250) (500,300) 1.5 1.5 bezier
animate_move l2 (560,280) (500,300) 1.5 1.5 bezier
animate_move l3 (560,310) (500,300) 1.5 1.5 bezier
animate_move l4 (560,340) (500,300) 1.5 1.5 bezier
animate_move l5 (560,370) (500,300) 1.5 1.5 bezier
animate_move l6 (580,370) (500,300) 1.5 1.5 bezier
animate_move l7 (590,370) (500,300) 1.5 1.5 bezier

animation_next_slide

# Slide 4: Flow Diagram with pulsing Start circle
animation_mode cycled
canvas_background (0.08,0.08,0.12,1.0) true (0.15,0.15,0.20,0.4)
text_create diagram_title "Flow Diagram" (300,60) (400,60) text_color #FFFFFF font "Ubuntu Bold 32"
shape_create start circle "Start" (150,280) (100,100) bg #10b981 text_color #FFFFFF font "Ubuntu Bold 18" filled true
shape_create process rectangle "Process" (400,250) (140,100) bg #3b82f6 text_color #FFFFFF font "Ubuntu Bold 18" filled true
shape_create end circle "End" (700,280) (100,100) bg #ef4444 text_color #FFFFFF font "Ubuntu Bold 18" filled true
connect start process straight single #94a3b8
connect process end straight single #94a3b8
# Pulsing animation that loops - keeping the circle smaller so it doesn't overlap arrow
animate_resize start (100,100) (115,115) 0.0 0.8 bezier
animate_resize start (115,115) (100,100) 0.8 0.8 bezier

animation_next_slide

# Slide 5: Thank You with star animations
animation_mode
canvas_background (0.05,0.15,0.1,1.0) true (0.1,0.2,0.15,0.4)
text_create thanks "Thank You!" (250,250) (500,150) text_color #10b981 font "Ubuntu Bold 64" align center
shape_create star1 circle "⭐" (150,450) (80,80) bg #fbbf24 filled false stroke 3 stroke_color #fbbf24
shape_create star2 circle "⭐" (450,450) (80,80) bg #fbbf24 filled false stroke 3 stroke_color #fbbf24
shape_create star3 circle "⭐" (750,450) (80,80) bg #fbbf24 filled false stroke 3 stroke_color #fbbf24
# Fade in text and stars
animate_appear thanks 0.0 1.5 bezier
animate_appear star1 1.5 0.5 bezier
animate_appear star2 1.8 0.5 bezier
animate_appear star3 2.1 0.5 bezier
