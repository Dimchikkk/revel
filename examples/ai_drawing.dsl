# AI Drawing - Car, Mountains, and Sun Scene

# Canvas settings - sky blue background
canvas_background (0.53,0.81,0.92,1.0) false

# Sun - bright yellow circle in top right
shape_create sun circle "" (800,80) (100,100) bg #FDB813 stroke 0 filled true

# Sun rays using lines
shape_create ray1 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 0
shape_create ray2 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 45
shape_create ray3 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 90
shape_create ray4 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 135
shape_create ray5 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 180
shape_create ray6 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 225
shape_create ray7 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 270
shape_create ray8 line "" (850,30) (60,3) bg #FDB813 stroke 4 rotation 315

# Layered mountains - back to front for depth
shape_create mountain_back triangle "" (80,280) (280,270) bg #3A5521 stroke 0 filled true
shape_create mountain_mid triangle "" (220,250) (300,300) bg #4A6B29 stroke 0 filled true
shape_create mountain_front triangle "" (400,270) (280,280) bg #5D8233 stroke 0 filled true

# Snow caps on peaks - perfectly aligned with mountain centers
shape_create snow1 triangle "" (310,250) (110,70) bg #FFFFFF stroke 0 filled true
shape_create snow2 triangle "" (500,270) (90,65) bg #FFFFFF stroke 0 filled true

# Ground - green grass
shape_create ground rectangle "" (0,450) (1100,250) bg #7CB342 stroke 0 filled true

# Road - simple gray asphalt
shape_create road rectangle "" (200,520) (300,180) bg #5A5A5A stroke 2 stroke_color #404040 filled true

# Road center lines - yellow dashes
shape_create line1 rectangle "" (340,530) (20,40) bg #FFEB3B stroke 0 filled true
shape_create line2 rectangle "" (340,590) (20,40) bg #FFEB3B stroke 0 filled true
shape_create line3 rectangle "" (340,650) (20,40) bg #FFEB3B stroke 0 filled true

# Car body - smooth rounded rectangle for modern look
shape_create car_body roundedrect "" (300,550) (180,70) bg #E53935 stroke 2 stroke_color #B71C1C filled true

# Car roof/cabin - also rounded
shape_create car_roof roundedrect "" (340,510) (100,50) bg #E53935 stroke 2 stroke_color #B71C1C filled true

# Car windows
shape_create window1 rectangle "" (350,515) (40,35) bg #81D4FA stroke 1 stroke_color #0277BD filled true
shape_create window2 rectangle "" (400,515) (35,35) bg #81D4FA stroke 1 stroke_color #0277BD filled true

# Car wheels - black circles
shape_create wheel1 circle "" (320,610) (40,40) bg #212121 stroke 2 stroke_color #000000 filled true
shape_create wheel2 circle "" (430,610) (40,40) bg #212121 stroke 2 stroke_color #000000 filled true

# Wheel hubs - gray circles
shape_create hub1 circle "" (330,620) (20,20) bg #9E9E9E stroke 0 filled true
shape_create hub2 circle "" (440,620) (20,20) bg #9E9E9E stroke 0 filled true

# Car headlight
shape_create headlight circle "" (470,575) (15,15) bg #FFF59D stroke 1 stroke_color #F9A825 filled true

# Clouds - fluffy white clouds using overlapping circles
shape_create cloud1_1 circle "" (150,100) (60,50) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true
shape_create cloud1_2 circle "" (180,95) (70,60) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true
shape_create cloud1_3 circle "" (210,100) (60,50) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true
shape_create cloud1_4 circle "" (235,105) (50,45) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true

shape_create cloud2_1 circle "" (600,140) (50,40) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true
shape_create cloud2_2 circle "" (625,135) (60,50) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true
shape_create cloud2_3 circle "" (650,140) (50,40) bg #FFFFFF stroke 1 stroke_color #E8E8E8 filled true

# Flying birds using bezier curves - elegant "V" shapes
# Bird 1 - left wing
shape_create bird1_left bezier "" (400,180) (35,25) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,1.0) p1=(0.3,0.2) p2=(0.7,0.0) p3=(1.0,0.5)
# Bird 1 - right wing
shape_create bird1_right bezier "" (435,180) (35,25) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,0.5) p1=(0.3,0.0) p2=(0.7,0.2) p3=(1.0,1.0)

# Bird 2 - smaller, higher
shape_create bird2_left bezier "" (480,140) (25,18) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,1.0) p1=(0.3,0.2) p2=(0.7,0.0) p3=(1.0,0.5)
shape_create bird2_right bezier "" (505,140) (25,18) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,0.5) p1=(0.3,0.0) p2=(0.7,0.2) p3=(1.0,1.0)

# Bird 3 - distant
shape_create bird3_left bezier "" (550,120) (20,15) bg #2C2C2C stroke 1 stroke_color #2C2C2C p0=(0.0,1.0) p1=(0.3,0.2) p2=(0.7,0.0) p3=(1.0,0.5)
shape_create bird3_right bezier "" (570,120) (20,15) bg #2C2C2C stroke 1 stroke_color #2C2C2C p0=(0.0,0.5) p1=(0.3,0.0) p2=(0.7,0.2) p3=(1.0,1.0)

# Bird 4 - near sun
shape_create bird4_left bezier "" (750,100) (22,16) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,1.0) p1=(0.3,0.2) p2=(0.7,0.0) p3=(1.0,0.5)
shape_create bird4_right bezier "" (772,100) (22,16) bg #2C2C2C stroke 2 stroke_color #2C2C2C p0=(0.0,0.5) p1=(0.3,0.0) p2=(0.7,0.2) p3=(1.0,1.0)

# Tree trunk
shape_create trunk rectangle "" (700,400) (30,80) bg #6D4C41 stroke 1 stroke_color #4E342E filled true

# Tree foliage - layered circles for depth
shape_create leaves1 circle "" (690,370) (75,75) bg #2E7D32 stroke 0 filled true
shape_create leaves2 circle "" (670,355) (60,60) bg #43A047 stroke 0 filled true
shape_create leaves3 circle "" (720,355) (60,60) bg #43A047 stroke 0 filled true
shape_create leaves4 circle "" (695,345) (65,65) bg #66BB6A stroke 0 filled true

# Title
text_create title "Scenic Drive" (50,30) (300,40) text_color #FFFFFF font "Ubuntu Bold 24"
