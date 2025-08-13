#!/bin/sh
# mp3_to_ogg.sh <input_mp3_file> <output_ogg_file>
ffmpeg -i $1 -c:a libopus -b:a 16k -ac 1 -ar 16000 -frame_duration 60 $2
