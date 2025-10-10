canvas_background (0.1,0.1,0.15,1.0) false

int grid_size 100
int max_index 99

int row 0
int col 0
int step 0

shape_create btn rectangle "Create Grid" (10,10) (150,40) filled true bg (0.3,0.6,0.9,1.0) text_color (1.0,1.0,1.0,1.0)

on click btn
  set step 1
end

on variable step == 1
  for row 0 max_index
    for col 0 max_index
      shape_create cell${row}${col} rectangle "" ({50+col*52},{70+row*52}) (48,48) filled true bg ({row/max_index},{col/max_index},0.5,1.0)
    end
  end
  set step 2
end
