// Demonstrate graph widget functions with two independent trace instances
// Multiple traces can be drawn at a time with multiple trace instances
// Note: Traces are automatically clipped at graph boundaries by widget library

// Requires widget library here:
// https://github.com/Bodmer/TFT_eWidget

#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

#include <TFT_eWidget.h>               // Widget library

GraphWidget gr = GraphWidget(&tft);    // Graph widget

// Traces are drawn on tft using graph instance
TraceWidget tr1 = TraceWidget(&gr);    // Graph trace 1
TraceWidget tr2 = TraceWidget(&gr);    // Graph trace 2

void setup() {
  Serial.begin(115200);
  delay(5000);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Graph area is 200 pixels wide, 150 high, dark grey background
  gr.createGraph(200, 150, tft.color565(5, 5, 5));

  // x scale units is from 0 to 100, y scale units is -50 to 50
  gr.setGraphScale(0.0, 100.0, -50.0, 50.0);

  // X grid starts at 0 with lines every 10 x-scale units
  // Y grid starts at -50 with lines every 25 y-scale units
  // blue grid
  gr.setGraphGrid(0.0, 10.0, -50.0, 25.0, TFT_BLUE);

  // Draw empty graph, top left corner at 40,10 on TFT
  gr.drawGraph(40, 10);

  // Start a trace with using red and another with green
  tr1.startTrace(TFT_RED);
  tr2.startTrace(TFT_GREEN);

  // Add points on graph to trace 1 using graph scale factors
  tr1.addPoint(0.0, 0.0);
  tr1.addPoint(100.0, 0.0);

  // Add points on graph to trace 2 using graph scale factors
  // Points are off graph so the plotted line is clipped to graph area
  tr2.addPoint(0.0, -100.0);
  tr2.addPoint(100.0, 100.0);

  // Get x,y pixel coordinates of any scaled point on graph
  // and ring that point.
  tft.drawCircle(gr.getPointX(50.0), gr.getPointY(0.0), 5, TFT_MAGENTA);

  // Draw the x axis scale
  tft.setTextDatum(TC_DATUM); // Top centre text datum
  tft.drawNumber(0, gr.getPointX(0.0), gr.getPointY(-50.0) + 3);
  tft.drawNumber(50, gr.getPointX(50.0), gr.getPointY(-50.0) + 3);
  tft.drawNumber(100, gr.getPointX(100.0), gr.getPointY(-50.0) + 3);

  // Draw the y axis scale
  tft.setTextDatum(MR_DATUM); // Middle right text datum
  tft.drawNumber(-50, gr.getPointX(0.0), gr.getPointY(-50.0));
  tft.drawNumber(0, gr.getPointX(0.0), gr.getPointY(0.0));
  tft.drawNumber(50, gr.getPointX(0.0), gr.getPointY(50.0));

  // Restart traces with new colours
  tr1.startTrace(TFT_WHITE);
  tr2.startTrace(TFT_YELLOW);
}

void loop() {
  static uint32_t plotTime = millis();
  static float gx = 0.0, gy = 0.0;
  static float delta = 7.0;

  // Sample periodically
  if (millis() - plotTime >= 100) {
    plotTime = millis();

    // Add a new point on each trace
    tr1.addPoint(gx, gy);
    tr2.addPoint(gx, gy/2.0); // half y amplitude

    // Create next plot point
    gx += 1.0;
    gy += delta;
    if (gy >  70.0) { delta = -7.0; gy =  70.0; }
    if (gy < -70.0) { delta =  7.0; gy = -70.0; }

    // If the end of the graph is reached start 2 new traces
    if (gx > 100.0) {
      gx = 0.0;
      gy = 0.0;

      // Draw empty graph at 40,10 on display
      gr.drawGraph(40, 10);
      // Start new trace
      tr1.startTrace(TFT_GREEN);
      tr2.startTrace(TFT_YELLOW);
    }
  }
}
