# DSL Demo for Blog Post - Showcasing Revel's Scripting Power

canvas_background (0.15,0.15,0.18,1.0) true (0.25,0.25,0.30,0.5)

# Title
note_create title "Revel DSL" (50,30) (300,80) bg #2563eb text_color #FFFFFF font "Ubuntu Bold 40"
text_create subtitle "Programmatic Canvas Creation" (380,40) (450,60) text_color #94a3b8 font "Ubuntu 18"

# Example 1: Shapes with different fill styles
paper_note_create demo1 "Fill Styles Demo" (50,150) (180,50) bg #f59e0b text_color #000000
shape_create c1 circle "Solid" (70,230) (100,100) bg #10b981 stroke 3 stroke_color #065f46 filled true fill_style solid text_color #FFFFFF font "Ubuntu Bold 14"
shape_create c2 circle "Hachure" (200,230) (100,100) bg #3b82f6 stroke 3 stroke_color #1e3a8a filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 14"
shape_create c3 circle "Cross" (330,230) (100,100) bg #8b5cf6 stroke 3 stroke_color #4c1d95 filled true fill_style crosshatch text_color #FFFFFF font "Ubuntu Bold 14"

# Example 2: Connection types
paper_note_create demo2 "Smart Connections" (500,150) (200,50) bg #06b6d4 text_color #000000
shape_create n1 rectangle "Start" (520,230) (80,60) bg #ef4444 filled true fill_style solid text_color #FFFFFF font "Ubuntu Bold 12"
shape_create n2 rectangle "Middle" (650,230) (80,60) bg #f59e0b filled true fill_style solid text_color #FFFFFF font "Ubuntu Bold 12"
shape_create n3 rectangle "End" (780,230) (80,60) bg #10b981 filled true fill_style solid text_color #FFFFFF font "Ubuntu Bold 12"
connect n1 n2 parallel single #FFFFFF
connect n2 n3 parallel double #FFFFFF

# Example 3: Rotation showcase
paper_note_create demo3 "Rotation Support" (900,150) (180,50) bg #8b5cf6 text_color #FFFFFF
shape_create r1 rectangle "0°" (920,240) (120,40) bg #14b8a6 filled true text_color #FFFFFF font "Ubuntu Bold 12" rotation 0
shape_create r2 rectangle "30°" (920,310) (120,40) bg #14b8a6 filled true text_color #FFFFFF font "Ubuntu Bold 12" rotation 30
shape_create r3 rectangle "60°" (920,380) (120,40) bg #14b8a6 filled true text_color #FFFFFF font "Ubuntu Bold 12" rotation 60

# Example 4: Complex workflow
paper_note_create demo4 "Build a Workflow" (50,400) (200,50) bg #f97316 text_color #000000
shape_create w1 diamond "Plan" (130,480) (80,70) bg #3b82f6 stroke 2 filled true text_color #FFFFFF font "Ubuntu Bold 11"
shape_create w2 roundedrect "Design" (130,580) (90,50) bg #8b5cf6 stroke 2 filled true text_color #FFFFFF font "Ubuntu Bold 11"
shape_create w3 vcylinder "Data" (250,520) (70,80) bg #06b6d4 stroke 2 filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 11"
shape_create w4 circle "Deploy" (130,680) (80,80) bg #10b981 stroke 2 filled true text_color #FFFFFF font "Ubuntu Bold 11"
connect w1 w2 parallel single #64748b
connect w2 w3 parallel single #64748b
connect w3 w4 parallel single #64748b
connect w2 w4 straight none #64748b60

# Example 5: Text with various styles
paper_note_create demo5 "Typography Control" (350,400) (250,50) bg #ec4899 text_color #FFFFFF
text_create t1 "Regular Text" (370,480) (200,40) text_color #e2e8f0 font "Ubuntu 16"
text_create t2 "Bold Text" (370,530) (200,40) text_color #fbbf24 font "Ubuntu Bold 16"
text_create t3 "Large Header" (370,580) (200,50) text_color #34d399 font "Ubuntu Bold 24"

# Footer note
paper_note_create footer "Created with Revel DSL\nAll in one command!" (650,580) (280,100) bg #1e293b text_color #94a3b8 font "Ubuntu 14"
