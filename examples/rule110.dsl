# Rule 110 - 500 cells × 500 generations
# Square cells for proper visualization

canvas_background (0.05,0.05,0.08,1.0) false

# 500 cells - start with rightmost = 1
int c[500] 0
set c[499] 1

int next[500] 0
int gen 0
int step 0
int i 0
int left 0
int right 0
int pattern 0

shape_create btn rectangle "START" (400,20) (150,50) filled true bg (0.2,0.6,0.9,1.0) text_color (1.0,1.0,1.0,1.0) font "Ubuntu Bold 20"
text_create lbl "Gen: 0 / 500" (570,30) (150,30) text_color (0.8,0.8,0.8,1.0) font "Ubuntu 16"
text_create title "Rule 110 - 500x500" (50,75) (500,40) text_color (1.0,1.0,1.0,1.0) font "Ubuntu Bold 24"

on click btn
  set step 1
end

# Step 1: Draw current generation with SQUARE cells
on variable step == 1
  set gen {gen + 1}
  for i 0 499
    shape_create r${gen}c${i} rectangle "" ({10+i*6},{120+gen*6}) (5,5) filled true bg {c[i],c[i],c[i],1.0}
  end
  text_update lbl "Gen: ${gen} / 500"
  for i 0 499
    set next[i] 0
  end
  set step 2
end

# Step 2: Apply Rule 110
on variable step == 2
  # Cell 0 (left edge)
  set left 0
  set right {c[1]}
  set pattern {left * 4 + c[0] * 2 + right}
  set next[0] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}

  # Cells 1-498 (middle)
  for i 1 498
    set left {c[i - 1]}
    set right {c[i + 1]}
    set pattern {left * 4 + c[i] * 2 + right}
    set next[i] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}
  end

  # Cell 499 (right edge)
  set left {c[498]}
  set right 0
  set pattern {left * 4 + c[499] * 2 + right}
  set next[499] {(pattern == 1) + (pattern == 2) + (pattern == 3) + (pattern == 5) + (pattern == 6)}

  set step 3
end

# Step 3: Copy and continue
on variable step == 3
  for i 0 499
    set c[i] {next[i]}
  end
  set step {(gen < 500) * 1}
end
