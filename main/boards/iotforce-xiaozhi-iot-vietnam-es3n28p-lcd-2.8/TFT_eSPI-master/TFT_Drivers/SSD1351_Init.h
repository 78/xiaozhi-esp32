{
  writecommand(0xFD); // COMMANDLOCK
  writedata(0x12);
  writecommand(0xFD); // COMMANDLOCK
  writedata(0xB1);
  writecommand(0xAE); // DISPLAYOFF
  writecommand(0xB3); // CLOCKDIV
  writedata(0xF1);
  writecommand(0xCA); // MUXRATIO
  writedata(127);
  writecommand(0xA2); // DISPLAYOFFSET
  writedata(0x00);
  writecommand(0xB5); // SETGPIO
  writedata(0x00);
  writecommand(0xAB); // FUNCTIONSELECT
  writedata(0x01);
  writecommand(0xB1); // PRECHARGE
  writedata(0x32);
  writecommand(0xBE); // VCOMH
  writedata(0x05);
  writecommand(0xA6); // NORMALDISPLAY
  writecommand(0xC1); // CONTRASTABC
  writedata(0xC8);
  writedata(0x80);
  writedata(0xC8);
  writecommand(0xC7); // CONTRASTMASTER
  writedata(0x0F);
  writecommand(0xB4); // SETVSL
  writedata(0xA0);
  writedata(0xB5);
  writedata(0x55);
  writecommand(0xB6); // PRECHARGE2
  writedata(0x01);
  writecommand(0xAF); // DISPLAYON
}
