# Test array support

canvas_background (0.1,0.1,0.12,1.0) false

int arr[5] 0
int i 0

# Set some values
set arr[0] 10
set arr[1] 20
set arr[2] 30
set arr[3] 40
set arr[4] 50

# Display array elements
for i 0 4
  shape_create box${i} rectangle "${arr[i]}" ({100+i*80},{100}) (60,40) filled true bg (0.3,0.5,0.7,1.0) text_color (1,1,1,1) font "Ubuntu Bold 16"
end

# Test computation
int sum 0
for i 0 4
  set sum {sum + arr[i]}
end

text_create sum_label "Sum: ${sum}" (100,200) (300,30) text_color (1,1,1,1) font "Ubuntu 18"
