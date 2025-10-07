canvas_background (0.05,0.05,0.1,1.0) true

int data_q1 1000
int data_q2 1500
int data_q3 1200
int data_q4 1800
int total {data_q1 + data_q2 + data_q3 + data_q4}

text_create title "Sales Dashboard" (400,50) (400,60) font "Ubuntu Bold 36"
text_create total_label "Total: ${total}" (400,120) (300,40) text_color #22c55e font "Ubuntu Bold 24"

# Bar chart
shape_create bar1 rectangle "Q1" (150,{600 - data_q1/5}) (80,{data_q1/5}) bg #3b82f6 filled true
shape_create bar2 rectangle "Q2" (280,{600 - data_q2/5}) (80,{data_q2/5}) bg #8b5cf6 filled true
shape_create bar3 rectangle "Q3" (410,{600 - data_q3/5}) (80,{data_q3/5}) bg #ec4899 filled true
shape_create bar4 rectangle "Q4" (540,{600 - data_q4/5}) (80,{data_q4/5}) bg #f59e0b filled true

# Make bars clickable to add sales
on click bar1
  add data_q1 100
end

on click bar2
  add data_q2 100
end

on click bar3
  add data_q3 100
end

on click bar4
  add data_q4 100
end

# Update visualizations
on variable data_q1
  animate_resize bar1 (80,{data_q1/5}) 0.0 0.4 bezier
  animate_move bar1 (150,{600 - data_q1/5}) 0.0 0.4 bezier
end

on variable data_q2
  animate_resize bar2 (80,{data_q2/5}) 0.0 0.4 bezier
  animate_move bar2 (280,{600 - data_q2/5}) 0.0 0.4 bezier
end

on variable data_q3
  animate_resize bar3 (80,{data_q3/5}) 0.0 0.4 bezier
  animate_move bar3 (410,{600 - data_q3/5}) 0.0 0.4 bezier
end

on variable data_q4
  animate_resize bar4 (80,{data_q4/5}) 0.0 0.4 bezier
  animate_move bar4 (540,{600 - data_q4/5}) 0.0 0.4 bezier
end

# Update total
on variable total
  text_update total_label "Total: ${total}"
end
