#!/bin/bash
# Script to record Revel as high-quality GIF or video
#
# USAGE:
#   1. Start Revel (with or without DSL)
#   2. Run this script: ./examples/record_animation.sh
#   3. Select the Revel window area when prompted
#   4. Perform your actions (navigate slides, create elements, etc.)
#   5. Press 'q' in the terminal to stop recording
#
# OUTPUT:
#   - If OUTPUT_FILE ends with .gif: Creates optimized GIF
#   - If OUTPUT_FILE ends with .mp4: Creates MP4 video (smaller file size)

# Configuration - adjust these as needed
OUTPUT_FILE="revel_recording.gif"  # Change extension to .mp4 for video output
FPS=30
SCALE=1200  # Width in pixels, height auto-calculated

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Revel Presentation Recorder${NC}"
echo "=============================="

# Check for required tools
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}Error: ffmpeg is not installed${NC}"
    echo "Install with: sudo apt install ffmpeg"
    exit 1
fi

if ! command -v slop &> /dev/null; then
    echo -e "${RED}Error: slop is not installed${NC}"
    echo "Install with: sudo apt install slop"
    exit 1
fi

# Use slop to select the region
echo -e "\n${YELLOW}Click and drag to select the Revel window area${NC}"
echo -e "${YELLOW}(You have 5 seconds to switch to the correct monitor)${NC}"
sleep 2

SELECTION=$(slop -f "%x %y %w %h")
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Region selection cancelled${NC}"
    exit 1
fi

read -r X Y WIDTH HEIGHT <<< "$SELECTION"

echo -e "${GREEN}Selected region:${NC}"
echo -e "${GREEN}  Position: ${X},${Y}${NC}"
echo -e "${GREEN}  Size: ${WIDTH}x${HEIGHT}${NC}"

# Countdown
echo -e "\n${YELLOW}Recording will start in:${NC}"
for i in 3 2 1; do
    echo -e "${YELLOW}  $i...${NC}"
    sleep 1
done

echo -e "${GREEN}Recording started!${NC}"
echo -e "${YELLOW}Press 'q' to stop recording${NC}"
echo ""

# Round coordinates to integers (slop may return floats)
X=$(printf "%.0f" "$X")
Y=$(printf "%.0f" "$Y")
WIDTH=$(printf "%.0f" "$WIDTH")
HEIGHT=$(printf "%.0f" "$HEIGHT")

# Make dimensions even (required by some encoders)
WIDTH=$((WIDTH - WIDTH % 2))
HEIGHT=$((HEIGHT - HEIGHT % 2))

echo -e "${YELLOW}Adjusted dimensions: ${WIDTH}x${HEIGHT}${NC}"

# Determine the correct DISPLAY variable
if [ -z "$DISPLAY" ]; then
    DISPLAY=":0"
fi

echo -e "${YELLOW}Using display: $DISPLAY${NC}"

# Record the video as MP4 (press 'q' to stop)
TEMP_VIDEO="temp_recording.mp4"

# Start ffmpeg in background and capture its PID
ffmpeg -video_size ${WIDTH}x${HEIGHT} \
       -framerate $FPS \
       -f x11grab \
       -i ${DISPLAY}+${X},${Y} \
       -c:v libx264 \
       -preset ultrafast \
       -crf 18 \
       -y \
       $TEMP_VIDEO 2>&1 &

FFMPEG_PID=$!

# Wait for user to press 'q' to stop
echo -e "${YELLOW}Recording in progress... (PID: $FFMPEG_PID)${NC}"
echo -e "${GREEN}Press 'q' and then Enter to stop${NC}"

# Wait for 'q' key
while true; do
    read -n 1 -s key
    if [[ $key == "q" ]]; then
        echo -e "\n${YELLOW}Stopping recording...${NC}"
        # Send 'q' to ffmpeg to stop gracefully
        kill -SIGINT $FFMPEG_PID
        wait $FFMPEG_PID 2>/dev/null
        break
    fi
done

# Give ffmpeg time to finalize the file
sleep 2

# Check if recording succeeded
if [ ! -f "$TEMP_VIDEO" ] || [ ! -s "$TEMP_VIDEO" ]; then
    echo -e "\n${RED}Error: Video recording failed${NC}"
    exit 1
fi

echo -e "\n${GREEN}Video recorded successfully${NC}"

# Check if output should be GIF or video
if [[ "$OUTPUT_FILE" == *.gif ]]; then
    echo -e "${YELLOW}Converting to optimized GIF...${NC}"

    # Convert to GIF with high quality palette
    ffmpeg -i $TEMP_VIDEO \
           -vf "fps=$FPS,scale=$SCALE:-1:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=256[p];[s1][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle" \
           -y \
           $OUTPUT_FILE 2>&1 | tail -10

    # Clean up temp file
    rm -f $TEMP_VIDEO

    # Check if conversion succeeded
    if [ -f "$OUTPUT_FILE" ] && [ -s "$OUTPUT_FILE" ]; then
        FILE_SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
        echo -e "\n${GREEN}Success!${NC}"
        echo -e "${GREEN}GIF saved as: $OUTPUT_FILE${NC}"
        echo -e "${GREEN}File size: $FILE_SIZE${NC}"
        echo ""
        echo "To view the GIF:"
        echo "  xdg-open $OUTPUT_FILE"
    else
        echo -e "\n${RED}Error: GIF conversion failed${NC}"
        exit 1
    fi
else
    # Keep as video (MP4)
    mv $TEMP_VIDEO $OUTPUT_FILE

    if [ -f "$OUTPUT_FILE" ] && [ -s "$OUTPUT_FILE" ]; then
        FILE_SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
        echo -e "\n${GREEN}Success!${NC}"
        echo -e "${GREEN}Video saved as: $OUTPUT_FILE${NC}"
        echo -e "${GREEN}File size: $FILE_SIZE${NC}"
        echo ""
        echo "To view the video:"
        echo "  xdg-open $OUTPUT_FILE"
    else
        echo -e "\n${RED}Error: Video save failed${NC}"
        exit 1
    fi
fi
