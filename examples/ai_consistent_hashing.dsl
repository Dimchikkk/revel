# Consistent Hashing - Educational Diagram

# Canvas setup
canvas_background (0.15,0.15,0.18,1.0) true (0.25,0.25,0.30,0.5)

# Title Section
note_create title "Consistent Hashing" (50,50) (400,60) bg #2563eb text_color #FFFFFF font "Ubuntu Bold 28"
paper_note_create intro "A distributed hashing scheme that minimizes\nkey remapping when nodes are added or removed" (470,50) (450,70) font "Ubuntu 14"

# Main Concept Section
text_create concept_title "The Hash Ring Concept" (50,150) (700,35) text_color #60a5fa font "Ubuntu Bold 22"

# Hash Ring Visualization (large circle)
# Ring center: (525, 420), radius: 200
shape_create ring circle "" (325,220) (400,400) bg #1e293b stroke 4 stroke_color #3b82f6 filled false

# Server Nodes on the Ring (positioned ON the perimeter)
# 45° clockwise from top: (666, 279) center -> (616, 229) top-left
shape_create server1 circle "Server A\nHash: 45°" (616,229) (100,100) bg #10b981 stroke 3 stroke_color #065f46 filled true text_color #FFFFFF font "Ubuntu Bold 12"
# 135° clockwise from top: (666, 561) center -> (616, 511) top-left
shape_create server2 circle "Server B\nHash: 135°" (616,511) (100,100) bg #f59e0b stroke 3 stroke_color #92400e filled true text_color #FFFFFF font "Ubuntu Bold 12"
# 225° clockwise from top: (384, 561) center -> (334, 511) top-left
shape_create server3 circle "Server C\nHash: 225°" (334,511) (100,100) bg #8b5cf6 stroke 3 stroke_color #4c1d95 filled true text_color #FFFFFF font "Ubuntu Bold 12"
# 315° clockwise from top: (384, 279) center -> (334, 229) top-left
shape_create server4 circle "Server D\nHash: 315°" (334,229) (100,100) bg #06b6d4 stroke 3 stroke_color #164e63 filled true text_color #FFFFFF font "Ubuntu Bold 12"

# Data Keys (smaller diamonds) - positioned inside the ring, not overlapping servers
shape_create key1 diamond "user_123" (550,340) (70,50) bg #ef4444 stroke 2 stroke_color #991b1b filled true text_color #FFFFFF font "Ubuntu 9"
shape_create key2 diamond "data_456" (430,410) (70,50) bg #ec4899 stroke 2 stroke_color #831843 filled true text_color #FFFFFF font "Ubuntu 9"
shape_create key3 diamond "item_789" (530,500) (70,50) bg #f97316 stroke 2 stroke_color #7c2d12 filled true text_color #FFFFFF font "Ubuntu 9"

# Ring direction indicator (at top of ring)
shape_create arrow_direction arrow "" (535,195) (50,15) bg #60a5fa stroke 3 stroke_style solid rotation 0
text_create clockwise "Clockwise" (595,195) (80,25) text_color #60a5fa font "Ubuntu Bold 11"

# Explanation boxes
paper_note_create explain1 "1. Hash Function:\n   • Servers & keys hashed to 0-360°\n   • Positioned on ring" (750,230) (280,100) font "Ubuntu 12"
paper_note_create explain2 "2. Key Assignment:\n   • Key goes to first server\n     clockwise from its position" (750,350) (280,100) font "Ubuntu 12"
paper_note_create explain3 "3. Load Balancing:\n   • Keys distributed evenly\n   • Minimal remapping on changes" (750,470) (280,100) font "Ubuntu 12"

# Connect keys to their assigned servers
connect key1 server1 straight single #ef444480
connect key2 server4 straight single #ec489980
connect key3 server2 straight single #f9731680

# Benefits Section
text_create benefits_title "Key Benefits" (50,640) (500,35) text_color #60a5fa font "Ubuntu Bold 22"

shape_create benefit1 roundedrect "Scalability" (50,690) (180,75) bg #10b981 stroke 3 stroke_color #065f46 filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 14"
shape_create benefit2 roundedrect "Fault Tolerance" (260,690) (180,75) bg #f59e0b stroke 3 stroke_color #92400e filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 14"
shape_create benefit3 roundedrect "Minimal Remapping" (470,690) (200,75) bg #8b5cf6 stroke 3 stroke_color #4c1d95 filled true fill_style hachure text_color #FFFFFF font "Ubuntu Bold 14"

paper_note_create benefit1_desc "Adding/removing nodes\naffects only neighboring\nkeys (~1/N keys)" (50,780) (180,90) font "Ubuntu 11"
paper_note_create benefit2_desc "Failed nodes' keys\nredistributed to next\navailable server" (260,780) (180,90) font "Ubuntu 11"
paper_note_create benefit3_desc "Traditional hashing:\nall keys remapped\nConsistent: ~1/N only" (470,780) (200,90) font "Ubuntu 11"

connect benefit1 benefit1_desc straight none #64748b60
connect benefit2 benefit2_desc straight none #64748b60
connect benefit3 benefit3_desc straight none #64748b60

# Scenario: Adding a New Server
text_create scenario_title "Scenario: Adding New Server" (50,900) (700,35) text_color #60a5fa font "Ubuntu Bold 22"

# Before state (smaller ring)
# Ring center: (250, 1100), radius: 100
text_create before_label "BEFORE: 4 Servers" (100,960) (200,30) text_color #94a3b8 font "Ubuntu Bold 13"
shape_create ring_before circle "" (150,1000) (200,200) bg #1e293b stroke 3 stroke_color #475569 filled false

# Servers on perimeter
shape_create srv_b1 circle "A" (301,1009) (40,40) bg #10b981 filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_b2 circle "B" (301,1151) (40,40) bg #f59e0b filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_b3 circle "C" (159,1151) (40,40) bg #8b5cf6 filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_b4 circle "D" (159,1009) (40,40) bg #06b6d4 filled true text_color #FFFFFF font "Ubuntu Bold 10"

shape_create key_before diamond "K1" (230,1070) (35,25) bg #ef4444 filled true text_color #FFFFFF font "Ubuntu 8"

# Arrow showing transition
shape_create transition_arrow arrow "" (360,1090) (100,30) bg #60a5fa stroke 4 stroke_style solid rotation 0
text_create add_server_label "Add Server E\nat 180°" (380,1120) (100,40) text_color #60a5fa font "Ubuntu Bold 11"

# After state
# Ring center: (650, 1100), radius: 100
text_create after_label "AFTER: 5 Servers" (500,960) (200,30) text_color #94a3b8 font "Ubuntu Bold 13"
shape_create ring_after circle "" (550,1000) (200,200) bg #1e293b stroke 3 stroke_color #3b82f6 filled false

# Servers on perimeter
shape_create srv_a1 circle "A" (701,1009) (40,40) bg #10b981 filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_a2 circle "B" (701,1151) (40,40) bg #f59e0b filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_a3 circle "C" (559,1151) (40,40) bg #8b5cf6 filled true text_color #FFFFFF font "Ubuntu Bold 10"
shape_create srv_a4 circle "D" (559,1009) (40,40) bg #06b6d4 filled true text_color #FFFFFF font "Ubuntu Bold 10"
# Server E at 180° (bottom of ring): center (650, 1200) -> top-left (630, 1180)
shape_create srv_a5 circle "E" (630,1180) (40,40) bg #14b8a6 filled true text_color #FFFFFF font "Ubuntu Bold 10"

shape_create key_after diamond "K1" (640,1090) (35,25) bg #ef4444 filled true text_color #FFFFFF font "Ubuntu 8"
connect key_after srv_a5 straight single #ef444480

paper_note_create impact "Only keys between D and E\nare remapped to E.\n\nAll other keys unaffected!" (550,1220) (200,85) bg #065f46 text_color #FFFFFF font "Ubuntu 11"

# Virtual Nodes Section
text_create virtual_title "Virtual Nodes Enhancement" (800,900) (350,35) text_color #60a5fa font "Ubuntu Bold 22"

paper_note_create virtual_explain "Problem: Uneven distribution\nwith few physical nodes\n\nSolution: Each server gets\nmultiple virtual node positions\non the ring (typically 100-200)" (800,950) (330,130) font "Ubuntu 12"

# Virtual nodes demo ring - center: (975, 1175), radius: 75
shape_create virtual_demo circle "" (900,1100) (150,150) bg #1e293b stroke 3 stroke_color #3b82f6 filled false
# A-v1 at 30° (NE): center x=975+75*cos(60°)=1012.5, y=1175-75*sin(60°)=1110
shape_create vnode1 circle "A-v1" (998,1095) (30,30) bg #10b981 filled true text_color #FFFFFF font "Ubuntu 8"
# A-v2 at 120° (SE): center x=975+75*cos(-30°)=1040, y=1175+75*sin(30°)=1212.5
shape_create vnode2 circle "A-v2" (1025,1198) (30,30) bg #10b981 filled true text_color #FFFFFF font "Ubuntu 8"
# A-v3 at 210° (SW): center x=975-75*cos(30°)=910, y=1175+75*sin(30°)=1212.5
shape_create vnode3 circle "A-v3" (895,1198) (30,30) bg #10b981 filled true text_color #FFFFFF font "Ubuntu 8"
# B-v1 at 300° (NW): center x=975-75*cos(60°)=937.5, y=1175-75*sin(60°)=1110
shape_create vnode4 circle "B-v1" (923,1095) (30,30) bg #f59e0b filled true text_color #FFFFFF font "Ubuntu 8"

text_create virtual_note "Same physical server,\nmultiple ring positions" (850,1260) (250,35) text_color #94a3b8 font "Ubuntu 11"

# Use Cases
text_create usecase_title "Common Use Cases" (50,1330) (700,35) text_color #60a5fa font "Ubuntu Bold 22"

shape_create use1 hcylinder "Distributed\nCaching" (50,1380) (200,75) bg #3b82f6 stroke 3 stroke_color #1e40af filled true text_color #FFFFFF font "Ubuntu Bold 13"
shape_create use2 hcylinder "Load\nBalancing" (280,1380) (200,75) bg #8b5cf6 stroke 3 stroke_color #6d28d9 filled true text_color #FFFFFF font "Ubuntu Bold 13"
shape_create use3 hcylinder "Distributed\nDatabases" (510,1380) (200,75) bg #10b981 stroke 3 stroke_color #059669 filled true text_color #FFFFFF font "Ubuntu Bold 13"
shape_create use4 hcylinder "CDN\nRouting" (740,1380) (200,75) bg #f59e0b stroke 3 stroke_color #d97706 filled true text_color #FFFFFF font "Ubuntu Bold 13"

# Footer
paper_note_create footer "Consistent Hashing ensures that when the number of nodes changes,\nonly a minimal fraction of keys need to be remapped, making it ideal\nfor scalable distributed systems." (50,1480) (900,75) bg #1e293b text_color #94a3b8 font "Ubuntu 13"
