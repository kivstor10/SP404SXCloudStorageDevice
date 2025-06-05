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
    showReadyToUpload();
}

void showSDRemoved(){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(38, 7);
    display.print("Please");
    display.setCursor(20, 17);
    display.print("Insert SD card");
    display.display();
}

void showReadyToUpload(){
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(35, 7);
    display.print("Ready to");
    display.setCursor(20, 17);
    display.print("download data...");
    display.display();
}

void showFileDownloadProgress(int currentFile, int percent) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.printf("File %d", currentFile);
    display.setCursor(0, 16);
    display.printf("progress: %d%%", percent);
    display.display();
}

void showLinkCode(const String& regCode) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(28, 10);
    display.print(regCode);
    display.display();
}
