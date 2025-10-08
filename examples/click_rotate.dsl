# Interactive Click to Rotate Demo
# Click on any shape to make it spin!

canvas_background (0.1,0.1,0.15,1.0) false

# Create shapes that rotate when clicked
shape_create box1 rectangle "Click Me!" (200,200) (120,120) bg #3b82f6 filled true text_color #ffffff font "Ubuntu Bold 14"
shape_create box2 roundedrect "Spin!" (400,200) (120,120) bg #ec4899 filled true text_color #ffffff font "Ubuntu Bold 14"
shape_create star circle "⭐" (600,200) (120,120) bg #f59e0b filled true text_color #ffffff font "Ubuntu Bold 48"
shape_create diamond diamond "💎" (200,400) (120,120) bg #10b981 filled true text_color #ffffff font "Ubuntu Bold 48"
shape_create triangle triangle "▲" (400,400) (120,120) bg #8b5cf6 filled true text_color #ffffff font "Ubuntu Bold 48"

# Click handlers - each click rotates the element 360 degrees
on click box1
  animate_rotate box1 360 0.0 0.8 bezier
end

on click box2
  animate_rotate box2 360 0.0 1.0 bezier
end

on click star
  animate_rotate star 360 0.0 0.6 linear
end

on click diamond
  animate_rotate diamond 720 0.0 1.2 bezier
end

on click triangle
  animate_rotate triangle -360 0.0 0.8 bezier
end

# Instructions
text_create instructions "Click on any shape to make it rotate!" (50,50) (700,30) text_color #94a3b8 font "Ubuntu 18"
