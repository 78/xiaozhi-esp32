---
name: Issue template
about: Guide to content
title: ''
labels: ''
assignees: ''

---

Only raise issues for problems with the library and/or provided examples. Post questions, comments and useful tips etc in the "Discussions" section.

To minimise effort to resolve issues the following should be provided as a minimum:
1. A description of the problem and the conditions that cause it to occur
2. IDE (e.g. Arduino or PlatformIO)
3. TFT_eSPI library version (try the latest, the problem may have been resolved!) from the Manage Libraries... menu
4. Board package version (e.g. 2.0.3) available from the Boards Manager... menu
5. Procesor, e.g RP2040, ESP32 S3 etc
6. TFT driver (e.g. ILI9341), a link to the vendors product web page is useful too.
7. Interface type (SPI or parallel)

Plus further information as appropriate to the problem:
1. TFT to processor connections used
2. A zip file containing your setup file (just drag and drop in message window - do not paste in long files!)
3. A zip file  containing a simple and complete example sketch that demonstrates the problem but needs no special hardware sensors or libraries.
4. Screen shot pictures showing the problem  (just drag and drop in message window)

The idea is to provide sufficient information so I can setup the exact same (or sufficiently similar) scenario to investigate and resolve the issue without having a tedious ping-pong of Q&A.

DO NOT paste code directly into the issue. To correctly format code put three ticks ( ` character on key next to "1" key) at the start and end of short pasted code segments to avoid format/markup anomolies. [See here:](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#quoting-code)

Example output:

```
  Serial.begin(115200);
  tft.init();
```
