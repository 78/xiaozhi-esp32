
// This is the command sequence that initialises the GC9A01 driver

{
  writecommand(0xEF);
  writecommand(0xEB);
  writedata(0x14);

  writecommand(0xFE);
  writecommand(0xEF);

  writecommand(0xEB);
  writedata(0x14);

  writecommand(0x84);
  writedata(0x40);

  writecommand(0x85);
  writedata(0xFF);

  writecommand(0x86);
  writedata(0xFF);

  writecommand(0x87);
  writedata(0xFF);

  writecommand(0x88);
  writedata(0x0A);

  writecommand(0x89);
  writedata(0x21);

  writecommand(0x8A);
  writedata(0x00);

  writecommand(0x8B);
  writedata(0x80);

  writecommand(0x8C);
  writedata(0x01);

  writecommand(0x8D);
  writedata(0x01);

  writecommand(0x8E);
  writedata(0xFF);

  writecommand(0x8F);
  writedata(0xFF);

  writecommand(0xB6);
  writedata(0x00);
  writedata(0x20);

  writecommand(0x3A);
  writedata(0x05);

  writecommand(0x90);
  writedata(0x08);
  writedata(0x08);
  writedata(0x08);
  writedata(0x08);

  writecommand(0xBD);
  writedata(0x06);

  writecommand(0xBC);
  writedata(0x00);

  writecommand(0xFF);
  writedata(0x60);
  writedata(0x01);
  writedata(0x04);

  writecommand(0xC3);
  writedata(0x13);
  writecommand(0xC4);
  writedata(0x13);

  writecommand(0xC9);
  writedata(0x22);

  writecommand(0xBE);
  writedata(0x11);

  writecommand(0xE1);
  writedata(0x10);
  writedata(0x0E);

  writecommand(0xDF);
  writedata(0x21);
  writedata(0x0c);
  writedata(0x02);

  writecommand(0xF0);
  writedata(0x45);
  writedata(0x09);
  writedata(0x08);
  writedata(0x08);
  writedata(0x26);
  writedata(0x2A);

  writecommand(0xF1);
  writedata(0x43);
  writedata(0x70);
  writedata(0x72);
  writedata(0x36);
  writedata(0x37);
  writedata(0x6F);

  writecommand(0xF2);
  writedata(0x45);
  writedata(0x09);
  writedata(0x08);
  writedata(0x08);
  writedata(0x26);
  writedata(0x2A);

  writecommand(0xF3);
  writedata(0x43);
  writedata(0x70);
  writedata(0x72);
  writedata(0x36);
  writedata(0x37);
  writedata(0x6F);

  writecommand(0xED);
  writedata(0x1B);
  writedata(0x0B);

  writecommand(0xAE);
  writedata(0x77);

  writecommand(0xCD);
  writedata(0x63);

  writecommand(0x70);
  writedata(0x07);
  writedata(0x07);
  writedata(0x04);
  writedata(0x0E);
  writedata(0x0F);
  writedata(0x09);
  writedata(0x07);
  writedata(0x08);
  writedata(0x03);

  writecommand(0xE8);
  writedata(0x34);

  writecommand(0x62);
  writedata(0x18);
  writedata(0x0D);
  writedata(0x71);
  writedata(0xED);
  writedata(0x70);
  writedata(0x70);
  writedata(0x18);
  writedata(0x0F);
  writedata(0x71);
  writedata(0xEF);
  writedata(0x70);
  writedata(0x70);

  writecommand(0x63);
  writedata(0x18);
  writedata(0x11);
  writedata(0x71);
  writedata(0xF1);
  writedata(0x70);
  writedata(0x70);
  writedata(0x18);
  writedata(0x13);
  writedata(0x71);
  writedata(0xF3);
  writedata(0x70);
  writedata(0x70);

  writecommand(0x64);
  writedata(0x28);
  writedata(0x29);
  writedata(0xF1);
  writedata(0x01);
  writedata(0xF1);
  writedata(0x00);
  writedata(0x07);

  writecommand(0x66);
  writedata(0x3C);
  writedata(0x00);
  writedata(0xCD);
  writedata(0x67);
  writedata(0x45);
  writedata(0x45);
  writedata(0x10);
  writedata(0x00);
  writedata(0x00);
  writedata(0x00);

  writecommand(0x67);
  writedata(0x00);
  writedata(0x3C);
  writedata(0x00);
  writedata(0x00);
  writedata(0x00);
  writedata(0x01);
  writedata(0x54);
  writedata(0x10);
  writedata(0x32);
  writedata(0x98);

  writecommand(0x74);
  writedata(0x10);
  writedata(0x85);
  writedata(0x80);
  writedata(0x00);
  writedata(0x00);
  writedata(0x4E);
  writedata(0x00);

  writecommand(0x98);
  writedata(0x3e);
  writedata(0x07);

  writecommand(0x35);
  writecommand(0x21);

  writecommand(0x11);
  delay(120);
  writecommand(0x29);
  delay(20);
}
