ECHO OFF
REM update_certs.cmd : update certs.ar with actual pems retrieved from mozilla
REM
REM see also %LOCALAPPDATA%\Arduino15\packages\esp8266\hardware\esp8266\3.0.2\libraries\ESP8266WiFi\examples\BearSSL_CertStore
REM
REM you may need to install openssl e.g. from https://kb.firedaemon.com/support/solutions/articles/4000121705
REM windows installer here: https://download.firedaemon.com/FireDaemon-OpenSSL/FireDaemon-OpenSSL-x64-3.0.7.exe
REM
SET PATH=%LOCALAPPDATA%\Arduino15\packages\esp8266\tools\python3\3.7.2-post1;%LOCALAPPDATA%\Arduino15\packages\esp8266\tools\xtensa-lx106-elf-gcc\3.0.4-gcc10.3-1757bed\xtensa-lx106-elf\bin;%PATH%
python3 certs-from-mozilla.py
REM
REM copy certs.ar manually to parent directory
REM
