# Revel DSL Demo - Infinite Canvas Note-Taking

# DSL quick reference (each command is one line):
#   canvas_background (bg_r,bg_g,bg_b,bg_a) SHOW_GRID (grid_r,grid_g,grid_b,grid_a)
#   note_create|paper_note_create ID "Text" (x,y) (w,h) [bg <color>] [text_color <color>] [font "Font Name"]
#   shape_create ID TYPE "Text" (x,y) (w,h) [bg <color>] [stroke <width>] [filled <bool>] [font "Font Name"] [text_color <color>]
#   image_create ID /abs/path/to.png (x,y) (w,h)
#   video_create ID /abs/path/to.mp4 (x,y) (w,h)
#   space_create ID "Name" (x,y) (w,h)
#   connect FROM_ID TO_ID [parallel|straight] [none|single|double] [color(...)|#RRGGBB[AA]|(r,g,b,a)]
#     • type/arrowhead default to parallel single; color defaults to white when omitted

# Canvas settings
canvas_background (0.95,0.95,0.98,1.0) true (0.85,0.85,0.90,0.5)

# Header notes
note_create title "Revel Canvas" (120,40) (320,80) bg #1f7fbf text_color #FFFFFF font "Ubuntu Bold 32"
paper_note_create subtitle "Organize ideas visually" (520,40) (320,60)

# Feature showcase with shapes
shape_create circle1 circle "Text Notes" (120,200) (160,160) bg #d0e8ff stroke 3 filled true text_color #1a2b50 font "Ubuntu Bold 16"
shape_create rect1 rectangle "Shapes" (380,200) (160,110) bg #ffe1e1 text_color #3f4f8f
shape_create vcyl1 vcylinder "Media" (640,200) (120,120) bg color(0.97,0.92,1.0,1.0) text_color #6d5ca6
shape_create hcyl1 hcylinder "Spaces" (900,210) (160,90) bg #f7e5b4 filled true text_color #4d3a1a

# Visual content
image_create img1 examples/media/cat.jpeg (140,440) (200,200)
video_create vid1 examples/media/blah_silent.mp4 (420,440) (320,220)

# Workspace section
space_create workspace "Projects" (860,440) (280,200)

# Create connections showing relationships with different styles
connect title circle1 parallel single color(0.20,0.60,0.85,1.0)
connect circle1 rect1 straight double #FFAA33
connect rect1 vcyl1
connect vcyl1 hcyl1 straight none color #FFFFFF80
connect circle1 img1 parallel single color (0.40,0.55,0.90,1.0)
connect img1 vid1 straight single color=rgba(0.25,0.80,0.45,1.0)
connect hcyl1 workspace parallel double color #FFD700
