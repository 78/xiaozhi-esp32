@echo off
set model=opus_data-%1.tar.gz

if not exist %model% (
    echo Downloading latest model
    powershell -Command "(New-Object System.Net.WebClient).DownloadFile('https://media.xiph.org/opus/models/%model%', '%model%')"
)

tar -xvzf %model%
