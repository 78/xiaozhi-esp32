# Copy into PTDB root directory and run to combine all the male/female raw audio/references into below directories

# Make folder for combined audio
mkdir -p './combined_mic_16k/'
# Make folder for combined pitch reference
mkdir -p './combined_reference_f0/'

# Resample Male Audio
for i in ./MALE/MIC/**/*.wav; do
j="$(basename "$i" .wav)"
echo $j
sox -r 48000 -b 16 -e signed-integer "$i" -r 16000 -b 16 -e signed-integer ./combined_mic_16k/$j.raw
done

# Resample Female Audio
for i in ./FEMALE/MIC/**/*.wav; do
j="$(basename "$i" .wav)"
echo $j
sox -r 48000 -b 16 -e signed-integer "$i" -r 16000 -b 16 -e signed-integer ./combined_mic_16k/$j.raw
done

# Shift Male reference pitch files
for i in ./MALE/REF/**/*.f0; do
j="$(basename "$i" .wav)"
echo $j
cp "$i" ./combined_reference_f0/
done

# Shift Female reference pitch files
for i in ./FEMALE/REF/**/*.f0; do
j="$(basename "$i" .wav)"
echo $j
cp "$i" ./combined_reference_f0/
done