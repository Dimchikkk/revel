# Slide 1: Welcome Adventure
animation_mode

global int correct_total 0
global int incorrect_total 0
global int planet_done 0
global int data_done 0
global string fieldnote_note ""

canvas_background (0.08,0.10,0.18,1.0) true (0.20,0.25,0.35,0.25)
text_create welcome_title "Habitat Explorer Briefing" (360,160) (520,70) text_color #fde68a font "Ubuntu Bold 34"
text_create welcome_msg "Complete three missions to help the park ranger file a science report." (360,240) (600,70) text_color #bfdbfe font "Ubuntu 20"
text_create hint "Stay focused on accuracy—the report tracks correct answers and reattempts." (360,300) (560,50) text_color #f8fafc font "Ubuntu 18"

animation_next_slide

# Slide 2: Science Check

text_create prompt "Mission 1: Select the planet known as the Red Planet." (360,220) (620,60) text_color #fde68a font "Ubuntu 20"
text_create hint_planet "Tap the correct badge to file the right answer." (360,270) (420,40) text_color #bae6fd font "Ubuntu 16"
shape_create option_mars roundedrect "Mars" (360,350) (220,60) bg #f97316 text_color #0f172a font "Ubuntu Bold 20" filled true
shape_create option_venus roundedrect "Venus" (360,430) (220,60) bg #fb7185 text_color #0f172a font "Ubuntu Bold 20" filled true
shape_create option_jupiter roundedrect "Jupiter" (360,510) (220,60) bg #38bdf8 text_color #0f172a font "Ubuntu Bold 20" filled true
text_create feedback "Awaiting your selection." (360,590) (420,50) text_color #f8fafc font "Ubuntu 18"
text_create score_panel "Score so far: ${correct_total} correct • ${incorrect_total} reattempts" (360,640) (460,40) text_color #bae6fd font "Ubuntu 18"

add planet_done -planet_done

on click option_mars
  text_update feedback "Correct! Mars is the Red Planet."
  add correct_total 1 - planet_done
  text_update score_panel "Score so far: ${correct_total} correct • ${incorrect_total} reattempts"
  add planet_done 1 - planet_done
end
on click option_venus
  text_update feedback "Not quite—try another planet."
  add correct_total -planet_done
  add incorrect_total 1
  text_update score_panel "Score so far: ${correct_total} correct • ${incorrect_total} reattempts"
end
on click option_jupiter
  text_update feedback "Not quite—try another planet."
  add correct_total -planet_done
  add incorrect_total 1
  text_update score_panel "Score so far: ${correct_total} correct • ${incorrect_total} reattempts"
end

on variable planet_done
  text_update score_panel "Score so far: ${correct_total} correct • ${incorrect_total} reattempts"
  presentation_auto_next_if planet_done 1
end

animation_next_slide

# Slide 3: Field Notes
text_create mission3 "Mission 2: Log a climate clue for the ranger." (360,140) (600,60) text_color #f8fafc font "Ubuntu 18"
text_create typing_score "Scoreboard: ${correct_total} correct • ${incorrect_total} reattempts" (360,200) (420,40) text_color #bae6fd font "Ubuntu Bold 18"
text_create instructions "Enter the lowercase term for water turning into vapor." (360,260) (540,40) text_color #fef9c3 font "Ubuntu 18"
text_create clipboard_label "Field note" (200,330) (200,40) text_color #38bdf8 font "Ubuntu Bold 18"
text_create fieldnote_entry "" (360,330) (320,60) bg #f8fafc text_color #0f172a font "Ubuntu 20"
text_bind fieldnote_entry fieldnote_note
text_create hint_lower "Hint: type it completely in lowercase before pressing Enter." (360,400) (520,40) text_color #c4b5fd font "Ubuntu 16"
text_create mission_tip "Field notes sync across slides automatically." (360,450) (520,40) text_color #f8fafc font "Ubuntu 16"

presentation_auto_next_if fieldnote_note "evaporation"

animation_next_slide

# Slide 4: Data Insight Check
add data_done -data_done

text_create data_title "Mission 3: Spot the accurate data card." (360,140) (580,50) text_color #f8fafc font "Ubuntu 18"
text_create data_score "Scoreboard: ${correct_total} correct • ${incorrect_total} reattempts" (360,190) (420,40) text_color #bae6fd font "Ubuntu Bold 18"
text_create data_feedback "Tap the card that matches the ranger's rainfall log." (360,240) (560,40) text_color #fef9c3 font "Ubuntu 18"
text_create data_context "Only one card reflects the actual field measurements for coastal herds." (360,280) (560,40) text_color #f8fafc font "Ubuntu 16"

shape_create card_rain roundedrect "Rainfall: 12 mm average" (240,360) (240,110) bg #38bdf8 text_color #0f172a font "Ubuntu Bold 18" filled true
shape_create card_altitude roundedrect "Altitude drives migration" (480,360) (240,110) bg #fb7185 text_color #0f172a font "Ubuntu Bold 18" filled true
shape_create card_food roundedrect "Food stores never change" (720,360) (240,110) bg #facc15 text_color #0f172a font "Ubuntu Bold 18" filled true

on click card_rain
  add correct_total 1 - data_done
  add data_done 1 - data_done
  text_update data_feedback "Nice catch—steady rainfall keeps the watering holes fresh."
  text_update data_score "Scoreboard: ${correct_total} correct • ${incorrect_total} reattempts"
end
on click card_altitude
  add incorrect_total 1
  text_update data_feedback "Altitude shifts weren't the key note in this survey—check again."
  text_update data_score "Scoreboard: ${correct_total} correct • ${incorrect_total} reattempts"
end
on click card_food
  add incorrect_total 1
  text_update data_feedback "The ranger reported food swings each week. Try another card."
  text_update data_score "Scoreboard: ${correct_total} correct • ${incorrect_total} reattempts"
end

on variable data_done
  presentation_auto_next_if data_done 1
end

animation_next_slide

# Slide 5: Mission Debrief
canvas_background (0.04,0.08,0.16,1.0) true (0.2,0.3,0.4,0.25)
text_create hooray "Mission accomplished, Explorer!" (360,200) (440,70) text_color #facc15 font "Ubuntu Bold 36"
text_create thanks "You supported the ranger with science knowledge and accurate field notes." (360,270) (600,60) text_color #bae6fd font "Ubuntu 20"
text_create final_score "Correct answers logged: ${correct_total}" (360,330) (460,40) text_color #f8fafc font "Ubuntu Bold 20"
text_create final_retries "Reattempts noted: ${incorrect_total}" (360,370) (460,40) text_color #f8fafc font "Ubuntu 18"
text_create fieldnote_summary "Field note recorded: ${fieldnote_note}" (360,410) (520,40) text_color #fef9c3 font "Ubuntu 18"
text_create accuracy "Accuracy rate: ${correct_total}/${correct_total + incorrect_total} actions" (360,450) (520,40) text_color #bae6fd font "Ubuntu 18"
text_create invite "Use Ctrl+Left Arrow to revisit any mission or fine-tune your answers." (360,500) (620,40) text_color #f8fafc font "Ubuntu 18"
