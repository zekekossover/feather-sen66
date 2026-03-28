/* This software is to work with the Exploratorium implementation
of the SEN66 from Sensiron and the Feather ESP32 S2 TFT Reverse*/

// these are libraries. If you don't already have these
// you may need to download them. The Arduino Library manager can help
#include <Arduino.h>
#include <SensirionI2cSen66.h>
#include <Wire.h>
#include <phyphoxBle.h> 

// more libraries but for graphics
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// macro definitions
// make sure that we use the proper definition of NO_ERROR
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensirionI2cSen66 sensor; // This declares (sets up) Sen66 and calls it sensor

static char errorMessage[64]; // variable to keep track of error messages
static int16_t error; // error number
int8_t serialNumber[32] = {0}; //variable to keep track of the serial number

String ident; // variable that uses part of the serialNumber to make a device ID
char charIdent[15]; // works with ident

// set up tasks are only run once when the device turns on
void setup() {
    //graphics on the display on the Feather
        // turn on backlite
       pinMode(TFT_BACKLITE, OUTPUT);
       digitalWrite(TFT_BACKLITE, HIGH);

        // turn on the TFT / I2C power supply
        pinMode(TFT_I2C_POWER, OUTPUT);
        digitalWrite(TFT_I2C_POWER, HIGH);
        delay(10);

        // initialize TFT
        tft.init(135, 240); // Init ST7789 240x135
        tft.setRotation(3);
        tft.fillScreen(ST77XX_BLACK);
    
    Serial.begin(115200); //This will send data when connected directly to a computer
    
    Wire.begin(); //connects to I2C communication
    sensor.begin(Wire, SEN66_I2C_ADDR_6B); //starts communication with the SEN66 Sensor

    //the following stuff writes error messages if the Sen66 doesn't work
    error = sensor.deviceReset();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    delay(1200);
    
    error = sensor.getSerialNumber(serialNumber, 32);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("serialNumber: ");
    Serial.print((const char*)serialNumber);
    Serial.println();
    error = sensor.startContinuousMeasurement();
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute startContinuousMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    
    //Phyphox Set up
        //This bit gives your device a name related to
        //the serial number. It allows you to use
        //many SEN66 at the same time.
        ident = "Device "+String((const char*)serialNumber);
        ident.toCharArray(charIdent,12);
        
        //setting up the phyphox app
        PhyphoxBLE::start(charIdent); //starting the phyphox with the name. It will appear in the Bluetooth menu
        PhyphoxBleExperiment MultiGraph; //creating a experiment called Multigraph
        MultiGraph.setTitle("SEN66"); 
        MultiGraph.setCategory("Arduino Experiments");
        MultiGraph.setDescription("Allows you to connect to a SEN66 with Bluetooth");

        PhyphoxBleExperiment::View firstView; //creates a set of graphs called firstView
        firstView.setLabel("PM"); //Create a "view"
        PhyphoxBleExperiment::Graph pmGraph; //create a graph called PM
        pmGraph.setLabel("Particulate Matter");
        pmGraph.setLabelX("time");
        pmGraph.setUnitX("s");
        pmGraph.setLabelY("PM2.5, PM10.0");
        
        pmGraph.setChannel(1,2); 
        /*In the main loop, the Feather will send five channels of info
        time, PM 2.5, PM 10, CO2, VOC in that order. So this graph will receive time and PM2.5 */
        pmGraph.setStyle(STYLE_DOTS);//"lines" are used if you dont set a style 
        pmGraph.setColor("ffffff"); //white
        pmGraph.setLinewidth(2);//if you dont select a linewidth, a width of 1 is used by default
        
        PhyphoxBleExperiment::Graph::Subgraph additionalData; //this allows two sets of data on one graph
        additionalData.setChannel(1,3); //This time we're sending the PM10 data
        additionalData.setStyle(STYLE_LINES);
        additionalData.setColor("ff00ff"); //magenta
        additionalData.setLinewidth(1);
  
        pmGraph.addSubgraph(additionalData);
        firstView.addElement(pmGraph);

        PhyphoxBleExperiment::Graph carbonDioxideGraph;
        carbonDioxideGraph.setLabel("Carbon dioxide");
        carbonDioxideGraph.setLabelX("time");
        carbonDioxideGraph.setUnitX("s");
        carbonDioxideGraph.setLabelY("CO2 PPM");
        
        carbonDioxideGraph.setChannel(1,4);
        carbonDioxideGraph.setStyle(STYLE_DOTS);//"lines" are used if you dont set a style 
        carbonDioxideGraph.setColor("ffffff");
        carbonDioxideGraph.setLinewidth(2);//if you dont select a linewidth, a width of 1 is used by default
        
        firstView.addElement(carbonDioxideGraph);

        PhyphoxBleExperiment::Graph vocGraph;
        vocGraph.setLabel("VOC");
        vocGraph.setLabelX("time");
        vocGraph.setUnitX("s");
        vocGraph.setLabelY("VOC Index");
        
        vocGraph.setChannel(1,5);
        vocGraph.setStyle(STYLE_DOTS);//"lines" are used if you dont set a style 
        vocGraph.setColor("ffffff");
        vocGraph.setLinewidth(2);//if you dont select a linewidth, a width of 1 is used by default
        
        firstView.addElement(vocGraph);

        MultiGraph.addView(firstView);
        PhyphoxBLE::addExperiment(MultiGraph); 
    
    
}

// loop runs over and over until the Feather is disconnected from power
void loop() {

    //we are creating some specialized variables with starting values
    float massConcentrationPm1p0 = 0.0;
    float massConcentrationPm2p5 = 0.0;
    float massConcentrationPm4p0 = 0.0;
    float massConcentrationPm10p0 = 0.0;
    float humidity = 0.0;
    float temperature = 0.0;
    float vocIndex = 0.0;
    float noxIndex = 0.0;
    uint16_t co2 = 0;
    
    delay(1000); //takes data once per second

    // the second part that says sensor.readMeasuredValues reads 
    // the Sen66 and assigns the values to the variables 
    // and sets an error case if something can't be read
    error = sensor.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, humidity, temperature, vocIndex, noxIndex,
        co2);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }

    // tft is the tiny screen on the Feather
    tft.setTextWrap(false);
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0); //set the cursor to the top
    tft.setTextColor(ST77XX_YELLOW);
    tft.setTextSize(2);

    Serial.print("massConcentrationPm1p0: "); // serial prints go the serial monitor
    Serial.print(massConcentrationPm1p0);
    Serial.print("\t"); //tab
    Serial.print("massConcentrationPm2p5: ");
    Serial.print(massConcentrationPm2p5);
    tft.print("2p5: "); // tft commands go to the screen on the feather
    tft.println(massConcentrationPm2p5);
    Serial.print("\t");
    Serial.print("massConcentrationPm4p0: ");
    Serial.print(massConcentrationPm4p0);
    Serial.print("\t");
    Serial.print("massConcentrationPm10p0: ");
    Serial.print(massConcentrationPm10p0);
    tft.print("10p0: ");
    tft.println(massConcentrationPm10p0);
    Serial.print("\t");
    Serial.print("humidity: ");
    Serial.print(humidity);
    tft.setTextColor(ST77XX_BLUE);
    tft.print("Humidity: ");
    tft.println(humidity);
    Serial.print("\t");
    Serial.print("temperature: ");
    Serial.print(temperature);
    tft.setTextColor(ST77XX_RED);
    tft.print("Temperature: ");
    tft.println(temperature);
    Serial.print("\t");
    Serial.print("vocIndex: ");
    Serial.print(vocIndex);
    Serial.print("\t");
    tft.setTextColor(ST77XX_MAGENTA);
    tft.print("VOC: ");
    tft.print(vocIndex,0);
    Serial.print("noxIndex: ");
    Serial.print(noxIndex);
    Serial.print("\t");
    tft.setTextColor(ST77XX_MAGENTA);
    tft.print("  NOX: ");
    tft.println(noxIndex,0);
    Serial.print("co2: ");
    Serial.print(co2);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("CO2 ");
    tft.println(co2);
    Serial.println();
    tft.setTextColor(ST77XX_YELLOW);
    tft.println();
    tft.println(charIdent);

    // this sends the data to phyphox via Bluetooth
    float currentTime = millis()/1000.0;
    float floatCO2 = co2; // the Sen66 outputs the CO2 level as integer but phyphox wants it as a float so we have to change it
    PhyphoxBLE::write(currentTime,massConcentrationPm2p5,massConcentrationPm10p0,floatCO2,vocIndex);

}

/* What follows are some required info
 * THIS FILE IS AUTOMATICALLY GENERATED
 *
 * Generator:     sensirion-driver-generator 1.1.2
 * Product:       sen66
 * Model-Version: 1.6.0
 */
/*
 * Some parts are Copyright (c) 2025, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
