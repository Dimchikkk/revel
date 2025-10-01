# Revel DSL Demo - Infinite Canvas Note-Taking

# DSL quick reference (each command is one line):
#   canvas_background (bg_r,bg_g,bg_b,bg_a) SHOW_GRID (grid_r,grid_g,grid_b,grid_a)
#   note_create|paper_note_create ID "Text" (x,y) (w,h) [bg <color>] [text_color <color>] [font "Font Name"] [align ALIGNMENT] [rotation DEGREES]
#   text_create ID "Text" (x,y) (w,h) [bg <color>] [text_color <color>] [font "Font Name"] [align ALIGNMENT] [rotation DEGREES]
#   shape_create ID TYPE "Text" (x,y) (w,h) [bg <color>] [stroke <width>] [stroke_color <color>] [filled <bool>] [fill_style solid|hachure|crosshatch] [font "Font Name"] [text_color <color>] [align ALIGNMENT] [rotation DEGREES]
#     • TYPE can be: circle, rectangle, triangle, diamond, vcylinder, hcylinder, roundedrect, line, arrow, bezier
#     • ALIGNMENT can be: top-left, top-center, top-right, center, bottom-left, bottom-right
#     • For bezier curves, add: [p0 (u,v)] [p1 (u,v)] [p2 (u,v)] [p3 (u,v)] where u,v are 0.0-1.0
#     • For line/arrow shapes, add: [line_start (u,v)] [line_end (u,v)] where u,v are 0.0-1.0
#   image_create ID /abs/path/to.png (x,y) (w,h) [rotation DEGREES]
#   video_create ID /abs/path/to.mp4 (x,y) (w,h) [rotation DEGREES]
#   space_create ID "Name" (x,y) (w,h) [rotation DEGREES]
#   connect FROM_ID TO_ID [parallel|straight] [none|single|double] [color(...)|#RRGGBB[AA]|(r,g,b,a)]
#     • type/arrowhead default to parallel single; color defaults to white when omitted

# Canvas settings
canvas_background (0.15,0.15,0.18,1.0) true (0.25,0.25,0.30,0.5)

# Header section
note_create title "Revel" (50,50) (280,70) bg #2563eb text_color #FFFFFF font "Ubuntu Bold 36"
paper_note_create subtitle "Professional brainstorming\nand note-taking" (360,30) (320,70)
text_create tagline "Infinite canvas • Rich media • Smart connections" (360,120) (500,30) text_color #94a3b8 font "Ubuntu 13"

# Shape types showcase - with varied fill patterns and stroke colors
shape_create s1 circle "Circle" (50,200) (120,120) bg #10b981 stroke 3 stroke_color #065f46 filled true text_color #FFFFFF font "Ubuntu Bold 14"
shape_create s2 rectangle "Rectangle" (200,200) (130,80) bg #f59e0b stroke 3 stroke_color #92400e filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 14"
shape_create s3 roundedrect "Rounded" (360,200) (130,80) bg #8b5cf6 stroke 3 stroke_color #4c1d95 filled true text_color #FFFFFF font "Ubuntu Bold 14"
shape_create s4 diamond "Diamond" (520,200) (110,100) bg #06b6d4 stroke 3 stroke_color #164e63 filled true text_color #FFFFFF font "Ubuntu Bold 14"
shape_create s5 vcylinder "V-Cyl" (660,200) (100,100) bg #f97316 stroke 3 stroke_color #7c2d12 filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 12" rotation 25
shape_create s6 hcylinder "H-Cyl" (800,210) (140,80) bg #14b8a6 stroke 3 stroke_color #134e4a filled true text_color #FFFFFF font "Ubuntu Bold 12"

# Arrow and line examples with different stroke styles and rotation showcase
shape_create arrow1 arrow "" (900,50) (180,40) bg #ef4444 stroke 3 stroke_style solid rotation 0
shape_create arrow2 arrow "" (900,120) (180,40) bg #3b82f6 stroke 3 stroke_style dashed rotation 0
shape_create line1 line "" (900,190) (180,20) bg #8b5cf6 stroke 3 stroke_style dotted rotation 0

# Media content
image_create img1 examples/media/cat.jpeg (50,410) (200,200)
video_create vid1 examples/media/blah_silent.mp4 (280,410) (320,200)

# Feature cards
paper_note_create feat1 "✓ Text formatting\n✓ Rich notes\n✓ Paper notes" (640,360) (200,100)
paper_note_create feat2 "✓ Images & video\n✓ Drag & drop\n✓ Media playback" (870,360) (200,100)
paper_note_create feat3 "✓ Smart arrows\n✓ Auto-routing\n✓ Connection points" (640,490) (200,100)
paper_note_create feat4 "✓ Nested spaces\n✓ Full-text search\n✓ SQLite database" (870,490) (200,100)

# Connection examples with different styles
connect title s1 parallel single #3b82f6
connect s1 s2 parallel single #f59e0b
connect s2 s3 parallel single #8b5cf6
connect s3 s4 parallel single #06b6d4
connect s4 s5 parallel single #f97316
connect s5 s6 parallel single #14b8a6
connect s1 img1 parallel single #64748b80
connect img1 vid1 straight single #64748b80
connect vid1 feat1 parallel single #64748b80
connect feat1 feat2 straight none #64748b60
connect feat2 feat4 straight none #64748b60
connect feat4 feat3 straight none #64748b60
