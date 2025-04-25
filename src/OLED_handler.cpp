#include "app.h"

void showDeviceLinked(){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(28, 7);
    display.print("Device linked");
    display.setCursor(30, 17);
    display.print("successfully!");
    display.display();

    delay(2500);
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(37, 7);
    display.print("Ready to");
    display.setCursor(28, 17);
    display.print("upload data...");
    display.display();
}