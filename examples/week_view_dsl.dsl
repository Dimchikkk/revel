# Weekly Interview Calendar - DSL Layout

# Canvas settings
canvas_background (0.15,0.15,0.18,1.0) true (0.25,0.25,0.30,0.5)

# Title
note_create title "Weekly Interview Schedule" (50,30) (1200,60) bg #1e40af text_color #FFFFFF font "Ubuntu Bold 28" align center

# Day headers
shape_create mon_header rectangle "Monday" (200,120) (150,50) bg #2563eb stroke 2 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create tue_header rectangle "Tuesday" (370,120) (150,50) bg #2563eb stroke 2 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create wed_header rectangle "Wednesday" (540,120) (150,50) bg #2563eb stroke 2 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create thu_header rectangle "Thursday" (710,120) (150,50) bg #2563eb stroke 2 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create fri_header rectangle "Friday" (880,120) (150,50) bg #2563eb stroke 2 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create sat_header rectangle "Saturday" (1050,120) (150,50) bg #475569 stroke 2 stroke_color #334155 filled true text_color #FFFFFF font "Ubuntu Bold 14" align center
shape_create sun_header rectangle "Sunday" (1220,120) (150,50) bg #475569 stroke 2 stroke_color #334155 filled true text_color #FFFFFF font "Ubuntu Bold 14" align center

# Time slots (left column)
text_create time_09 "09:00" (50,200) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_10 "10:00" (50,260) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_11 "11:00" (50,320) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_12 "12:00" (50,380) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_13 "13:00" (50,440) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_14 "14:00" (50,500) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_15 "15:00" (50,560) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_16 "16:00" (50,620) (120,50) text_color #94a3b8 font "Ubuntu 13" align center
text_create time_17 "17:00" (50,680) (120,50) text_color #94a3b8 font "Ubuntu 13" align center

# Grid lines (optional visual guides)
shape_create grid1 line "" (200,190) (1170,2) bg #334155 stroke 1
shape_create grid2 line "" (200,250) (1170,2) bg #334155 stroke 1
shape_create grid3 line "" (200,310) (1170,2) bg #334155 stroke 1
shape_create grid4 line "" (200,370) (1170,2) bg #334155 stroke 1
shape_create grid5 line "" (200,430) (1170,2) bg #334155 stroke 1
shape_create grid6 line "" (200,490) (1170,2) bg #334155 stroke 1
shape_create grid7 line "" (200,550) (1170,2) bg #334155 stroke 1
shape_create grid8 line "" (200,610) (1170,2) bg #334155 stroke 1
shape_create grid9 line "" (200,670) (1170,2) bg #334155 stroke 1
shape_create grid10 line "" (200,730) (1170,2) bg #334155 stroke 1

# Example interview slots - add your interviews here!

# Monday
paper_note_create mon_10 "Technical Interview\nCandidate: John Doe\nPosition: Senior Dev" (200,260) (150,110) bg #10b981 text_color #FFFFFF

# Tuesday
paper_note_create tue_14 "Phone Screen\nCandidate: Jane Smith\nPosition: Designer" (370,500) (150,110) bg #f59e0b text_color #FFFFFF

# Wednesday
paper_note_create wed_11 "System Design\nCandidate: Alex Chen\nPosition: Architect" (540,320) (150,110) bg #8b5cf6 text_color #FFFFFF

# Thursday - Back-to-back interviews
paper_note_create thu_09 "First Round\nCandidate: Sarah Lee\nPosition: Frontend" (710,200) (150,110) bg #06b6d4 text_color #FFFFFF
paper_note_create thu_10 "Panel Interview\nCandidate: Mike Brown\nPosition: Backend" (710,260) (150,170) bg #f97316 text_color #FFFFFF

# Friday
paper_note_create fri_15 "Final Interview\nCandidate: Emily Davis\nPosition: Manager" (880,560) (150,110) bg #ec4899 text_color #FFFFFF

# Legend
shape_create legend_bg rectangle "" (50,755) (700,40) bg #1e293b stroke 2 stroke_color #334155 filled true
text_create legend "Interview Status:" (70,760) (150,30) text_color #94a3b8 font "Ubuntu Bold 14"
shape_create leg1 circle "" (240,765) (20,20) bg #10b981 filled true
text_create leg1_txt "Scheduled" (270,760) (100,30) text_color #94a3b8 font "Ubuntu 12"
shape_create leg2 circle "" (390,765) (20,20) bg #f59e0b filled true
text_create leg2_txt "Pending" (420,760) (100,30) text_color #94a3b8 font "Ubuntu 12"
shape_create leg3 circle "" (540,765) (20,20) bg #8b5cf6 filled true
text_create leg3_txt "Confirmed" (570,760) (100,30) text_color #94a3b8 font "Ubuntu 12"

# Add note for customization
paper_note_create instructions "Add your interviews:\n• Copy existing interview blocks\n• Modify time/day positions\n• Update candidate details\n• Change colors for status" (800,760) (400,100) bg #1e293b text_color #94a3b8
