# Smart thermostat control demo
canvas_background (0.08,0.09,0.12,1.0) true (0.15,0.17,0.22,0.5)

int current_temp 68
int min_temp 60
int max_temp 80
int comfort_span {max_temp - min_temp}

text_create heading "Thermostat" (420,60) (360,60) text_color #e2e8f0 font "Ubuntu Bold 36"
text_create subheading "Tap buttons to adjust" (420,110) (360,40) text_color #94a3b8 font "Ubuntu 16"
text_create readout "Temp: ${current_temp}°F" (200,320) (220,60) text_color #22d3ee font "Ubuntu Bold 28"
text_create min_label "${min_temp}°F" (420,500) (120,40) text_color #64748b font "Ubuntu 14"
text_create max_label "${max_temp}°F" (420,150) (120,40) text_color #64748b font "Ubuntu 14"

# Gauge background + fill (fill height scales with temperature)
shape_create gauge_bg roundedrect "" (420,180) (120,300) bg #1f2937 stroke 2 stroke_color #334155 filled true
shape_create gauge_fill roundedrect "" (420,{180 + (max_temp - current_temp) * 300 / comfort_span}) (120,{(current_temp - min_temp) * 300 / comfort_span}) bg #22d3ee filled true

# Buttons
shape_create up_btn circle "▲" (640,220) (90,90) bg #10b981 filled true font "Ubuntu Bold 36" text_color #0f172a
shape_create down_btn circle "▼" (640,360) (90,90) bg #f97316 filled true font "Ubuntu Bold 36" text_color #0f172a

# Button interactions
on click up_btn
  add current_temp 1
end

on click down_btn
  add current_temp -1
end

# Clamp and feedback when temperature changes
on variable current_temp
  text_update readout "Temp: ${current_temp}°F"
  animate_resize gauge_fill (120,{(current_temp - min_temp) * 300 / comfort_span}) 0.0 0.35 bezier
  animate_move gauge_fill (420,{180 + (max_temp - current_temp) * 300 / comfort_span}) 0.0 0.35 bezier
end
