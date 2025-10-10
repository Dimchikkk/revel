# Slide 1: Welcome
animation_mode

global int correct_total 0
global int incorrect_total 0
global int planet_done 0
global int data_done 0
global string fieldnote_note ""

canvas_background (0.95,0.96,0.97,1.0) false (0,0,0,0)

text_create welcome_title "Environmental Science Quiz" (360,100) (600,50) text_color #1e40af font "Ubuntu Bold 36"
text_create subtitle "Assessment Module" (360,160) (400,30) text_color #6b7280 font "Ubuntu 20"
text_create instructions "Complete three questions to demonstrate your understanding of environmental science concepts." (360,240) (560,60) text_color #374151 font "Ubuntu 18"
text_create hint "Each question tracks accuracy. Take your time and select the best answer." (360,320) (540,50) text_color #6b7280 font "Ubuntu 16"
text_create ready "Click the arrow to begin →" (360,400) (300,30) text_color #9ca3af font "Ubuntu Italic 16"

animation_next_slide

# Slide 2: Question 1 - Astronomy

text_create q1_title "Question 1 of 3" (360,80) (300,40) text_color #1e40af font "Ubuntu Bold 28"
text_create q1_score "Score: ${correct_total} correct • ${incorrect_total} incorrect" (360,130) (400,30) text_color #6b7280 font "Ubuntu 16"

text_create prompt "Which planet is known as the Red Planet?" (360,200) (540,40) text_color #1f2937 font "Ubuntu Bold 22"

shape_create option_mars roundedrect "Mars" (360,280) (280,60) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 18" filled true
shape_create option_venus roundedrect "Venus" (360,360) (280,60) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 18" filled true
shape_create option_jupiter roundedrect "Jupiter" (360,440) (280,60) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 18" filled true

text_create feedback "Select your answer" (360,540) (440,40) text_color #6b7280 font "Ubuntu Italic 16"

set planet_done {planet_done - planet_done}

on click option_mars
  text_update feedback "✓ Correct! Mars is the Red Planet."
  set correct_total {correct_total + 1 - planet_done}
  text_update q1_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
  set planet_done {planet_done + 1 - planet_done}
end
on click option_venus
  text_update feedback "✗ Incorrect. Try again."
  set correct_total {correct_total - planet_done}
  set incorrect_total {incorrect_total + 1}
  text_update q1_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
end
on click option_jupiter
  text_update feedback "✗ Incorrect. Try again."
  set correct_total {correct_total - planet_done}
  set incorrect_total {incorrect_total + 1}
  text_update q1_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
end

on variable planet_done
  text_update q1_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
  presentation_auto_next_if planet_done 1
end

animation_next_slide

# Slide 3: Question 2 - Climate Science

text_create q2_title "Question 2 of 3" (360,80) (300,40) text_color #1e40af font "Ubuntu Bold 28"
text_create q2_score "Score: ${correct_total} correct • ${incorrect_total} incorrect" (360,130) (400,30) text_color #6b7280 font "Ubuntu 16"

text_create q2_prompt "What is the term for water turning into vapor?" (360,200) (540,40) text_color #1f2937 font "Ubuntu Bold 22"
text_create q2_hint "(Enter answer in lowercase)" (360,250) (400,30) text_color #6b7280 font "Ubuntu 16"

text_create clipboard_label "Your Answer:" (360,320) (200,30) text_color #374151 font "Ubuntu Bold 16"
text_create fieldnote_entry "" (360,370) (400,50) bg #ffffff text_color #1f2937 font "Ubuntu 20"
text_bind fieldnote_entry fieldnote_note

text_create hint_lower "Press Enter to submit your answer" (360,440) (400,30) text_color #9ca3af font "Ubuntu Italic 14"

presentation_auto_next_if fieldnote_note "evaporation"

animation_next_slide

# Slide 4: Question 3 - Ecology

text_create q3_title "Question 3 of 3" (360,80) (300,40) text_color #1e40af font "Ubuntu Bold 28"
text_create q3_score "Score: ${correct_total} correct • ${incorrect_total} incorrect" (360,130) (400,30) text_color #6b7280 font "Ubuntu 16"

text_create data_title "Which statement is supported by coastal herd data?" (360,200) (560,40) text_color #1f2937 font "Ubuntu Bold 22"
text_create data_context "Select the observation that matches field measurements." (360,250) (540,30) text_color #6b7280 font "Ubuntu 16"

shape_create card_rain roundedrect "Average rainfall: 12mm" (360,340) (180,80) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 15" filled true
shape_create card_altitude roundedrect "Altitude drives migration" (560,340) (180,80) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 15" filled true
shape_create card_food roundedrect "Food supply is constant" (760,340) (180,80) bg #3b82f6 text_color #ffffff font "Ubuntu Bold 15" filled true

text_create data_feedback "Select your answer" (360,460) (480,40) text_color #6b7280 font "Ubuntu Italic 16"

set data_done {data_done - data_done}

on click card_rain
  set correct_total {correct_total + 1 - data_done}
  text_update data_feedback "✓ Correct! Consistent rainfall maintains water sources."
  text_update q3_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
  set data_done {data_done + 1 - data_done}
end
on click card_altitude
  set incorrect_total {incorrect_total + 1}
  text_update data_feedback "✗ Incorrect. Altitude wasn't the primary factor."
  text_update q3_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
end
on click card_food
  set incorrect_total {incorrect_total + 1}
  text_update data_feedback "✗ Incorrect. Food supply fluctuates weekly."
  text_update q3_score "Score: ${correct_total} correct • ${incorrect_total} incorrect"
end

on variable data_done
  presentation_auto_next_if data_done 1
end

animation_next_slide

# Slide 5: Results

text_create results_title "Quiz Complete" (360,80) (400,50) text_color #059669 font "Ubuntu Bold 36"
text_create summary "You have completed the Environmental Science Quiz." (360,150) (560,40) text_color #374151 font "Ubuntu 20"

text_create score_header "Performance Summary" (360,210) (300,30) text_color #1e40af font "Ubuntu Bold 22"

shape_create correct_box roundedrect "" (340,280) (180,100) bg #d1fae5 text_color #065f46 font "Ubuntu Bold 24" filled true
text_create correct_label "Correct" (340,245) (120,25) text_color #065f46 font "Ubuntu Bold 16"
text_create final_score "${correct_total}" (340,300) (100,50) text_color #065f46 font "Ubuntu Bold 48"

shape_create incorrect_box roundedrect "" (580,280) (180,100) bg #fee2e2 text_color #991b1b font "Ubuntu Bold 24" filled true
text_create incorrect_label "Incorrect" (580,245) (120,25) text_color #991b1b font "Ubuntu Bold 16"
text_create final_retries "${incorrect_total}" (580,300) (100,50) text_color #991b1b font "Ubuntu Bold 48"

text_create accuracy_label "Accuracy Rate" (360,400) (200,30) text_color #6b7280 font "Ubuntu Bold 18"
text_create accuracy "${correct_total}/${correct_total + incorrect_total}" (360,440) (200,40) text_color #1f2937 font "Ubuntu Bold 32"

text_create invite "Use Ctrl+Left Arrow to review questions" (360,520) (480,30) text_color #9ca3af font "Ubuntu 14"
