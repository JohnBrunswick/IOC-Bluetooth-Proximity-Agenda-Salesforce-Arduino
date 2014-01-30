/*
Code sample based on the following resources
- Bluetooth Detection http://www.fritz-hut.com/2012/08/26/bluetooth-presence-detection/
- WiFi http://arduino.cc/en/Tutorial/WiFiWebClientRepeating
- HTTP PUT https://www.johnbrunswick.com/2014/01/internet-of-customers-arduino-rfid-http-put-node-js/
 */
#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFi.h>
#include <string.h>

char ssid[] = "xxxxx"; // your network SSID (name) 
char key[] = "xxxxx"; // your network key

// Keep track of web connectivity
boolean lastConnected = false;
boolean incomingData = false;
int status = WL_IDLE_STATUS; // the Wifi radio's status
int keyIndex = 0; // your network key Index number

// Take care of advoiding duplicate posts
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 10*1000;

// Removing duplicate posts to Node.js service within certain interval
unsigned long lastReadTime = 0;
const unsigned long pollingInterval = 10*1000;

IPAddress server(192,168,1,103);  // numeric IP (no DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;
unsigned long lastTime;
unsigned long interval = 10000;

int bluetoothTx = 2;  // TX-O pin of bluetooth mate, Arduino D2
int bluetoothRx = 3;  // RX-I pin of bluetooth mate, Arduino D3

SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);

String btid;

void setup()
{
  Serial.begin(9600);  // Begin the serial monitor at 9600bps

  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");  // Print three times individually
  bluetooth.print("$");
  bluetooth.print("$");  // Enter command mode
  delay(100);  // Short delay, wait for the Mate to send back CMD
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  // 115200 can be too fast at times for NewSoftSerial to relay the data reliably
  bluetooth.begin(9600);  // Start bluetooth serial at 9600
  
  Serial.println("------------------");
  Serial.println("Starting - RFID and WiFi POC Test");

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, keyIndex, key);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi...");
  Serial.println("------------------");
  Serial.println("");

  printWifiStatus();
}

void loop()
{
    // Get current millis();
    unsigned long currentTime = millis();
    // Check if its already time for our process
    if((currentTime - lastTime) > interval) {
        lastTime = currentTime;
        // Enter command mode
        bluetooth.print("$$$");
                //Wait a bit for the module to respond
        delay(200);
                // Start the inquiry scan
        bluetooth.println("IN,2");
        delay(5000);
        delay(500);
        int h = bluetooth.available();
        if(h >= 42) { // we know there is a bluetooth device
            Serial.println("Someone is near...");
            char *id = readSerial(h);
            char *str;
            
            while((str = strtok_r(id, "n", &id)) != NULL) {
                    Serial.println(str);
                      
                      // TODO - find better way to deal with null at end of str
                      // variable to assign for later use as string
                      btid = (String)str;
                      btid = btid.substring(0, 19);

                  while (client.available()) {
                    char c = client.read();
                    Serial.write(c);
                  }
                  if (!client.connected() && lastConnected) {
                    Serial.println();
                    Serial.println("disconnecting.");
                    client.stop();
                  }
                  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
                    sendData();
                  }
                  lastConnected = client.connected();
            }
        } else if(h <= 21) { // we know there is no bluetooth device
            Serial.println("No device found");
        }
        bluetooth.println("---");

    }
    clearAll();
}

void clearAll() {
    for(int i = 0; i < bluetooth.available(); i++) {
        bluetooth.read();
    }
}

// a bit hacked...
char* readSerial(int h) {
    char input;
    char buffer[100];
    if (bluetooth.available() > 0) {
        int j = 0;
        for (int i = 0; i < h; i++) { // the id's start at char 21, se we copy from there
            input = (char)bluetooth.read();
            if(i >= 21) {
                buffer[j] = input;
                buffer[j+1] = '';
                j++;
            }
        }
        return buffer;
    } else {
        return "No Data";
    }
}

void sendData() {
  Serial.println("----------------->");
  Serial.println("Trying to send card data to server for card " + btid + "...");

  // if you get a connection, report back via serial:
  // change port info as needed, this was used with a local instance via Heroku command line
  if (client.connect(server, 3001)) {
  
    Serial.println("Connected to server...");
    String feedData = "n{\"mobiledevice\" : {\"btid\" : \"" + btid + "\"}}";

    Serial.println("Sending: " + feedData + "...");
    
    client.println("PUT /checkin/ HTTP/1.0");

    client.println("Host: 192.168.1.103");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(feedData.length()));
    client.print("Connection: close");
    client.println();
    client.print(feedData);
    client.println();    

    Serial.println("Data has been sent...");
    Serial.println("----------------->");

    delay(2000); 
    
    lastConnectionTime = millis();
        
    while (client.available() && status == WL_CONNECTED) {
        if (incomingData == false)
        {
          Serial.println();
          Serial.println("--- Incoming data Start ---");
          incomingData = true;
        }
        char c = client.read();
        Serial.write(c);
        client.flush();
        client.stop();
    }

    if (incomingData == true) {
      Serial.println("--- Incoming data End");
      incomingData = false; 
    }
    if (status == WL_CONNECTED) {
      Serial.println("--- WiFi connected");
    }
    else {
      Serial.println("--- WiFi not connected");
    }
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
  
  // Reset Bluetooth ID
  btid = "";
}

void printWifiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.println("------------------");
  Serial.println("WiFi Status...");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal Strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("------------------");
  Serial.println("");
}/*
Code sample based on the following resources
- Bluetooth Detection http://www.fritz-hut.com/2012/08/26/bluetooth-presence-detection/
- WiFi http://arduino.cc/en/Tutorial/WiFiWebClientRepeating
- HTTP PUT https://www.johnbrunswick.com/2014/01/internet-of-customers-arduino-rfid-http-put-node-js/
 */
#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFi.h>
#include <string.h>

char ssid[] = "xxxxx"; // your network SSID (name) 
char key[] = "xxxxx"; // your network key

// Keep track of web connectivity
boolean lastConnected = false;
boolean incomingData = false;
int status = WL_IDLE_STATUS; // the Wifi radio's status
int keyIndex = 0; // your network key Index number

// Take care of advoiding duplicate posts
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 10*1000;

// Removing duplicate posts to Node.js service within certain interval
unsigned long lastReadTime = 0;
const unsigned long pollingInterval = 10*1000;

IPAddress server(192,168,1,103);  // numeric IP (no DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;
unsigned long lastTime;
unsigned long interval = 10000;

int bluetoothTx = 2;  // TX-O pin of bluetooth mate, Arduino D2
int bluetoothRx = 3;  // RX-I pin of bluetooth mate, Arduino D3

SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);

String btid;

void setup()
{
  Serial.begin(9600);  // Begin the serial monitor at 9600bps

  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");  // Print three times individually
  bluetooth.print("$");
  bluetooth.print("$");  // Enter command mode
  delay(100);  // Short delay, wait for the Mate to send back CMD
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  // 115200 can be too fast at times for NewSoftSerial to relay the data reliably
  bluetooth.begin(9600);  // Start bluetooth serial at 9600
  
  Serial.println("------------------");
  Serial.println("Starting - RFID and WiFi POC Test");

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, keyIndex, key);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi...");
  Serial.println("------------------");
  Serial.println("");

  printWifiStatus();
}

void loop()
{
    // Get current millis();
    unsigned long currentTime = millis();
    // Check if its already time for our process
    if((currentTime - lastTime) > interval) {
        lastTime = currentTime;
        // Enter command mode
        bluetooth.print("$$$");
                //Wait a bit for the module to respond
        delay(200);
                // Start the inquiry scan
        bluetooth.println("IN,2");
        delay(5000);
        delay(500);
        int h = bluetooth.available();
        if(h >= 42) { // we know there is a bluetooth device
            Serial.println("Someone is near...");
            char *id = readSerial(h);
            char *str;
            
            while((str = strtok_r(id, "n", &id)) != NULL) {
                    Serial.println(str);
                      
                      // TODO - find better way to deal with null at end of str
                      // variable to assign for later use as string
                      btid = (String)str;
                      btid = btid.substring(0, 19);

                  while (client.available()) {
                    char c = client.read();
                    Serial.write(c);
                  }
                  if (!client.connected() && lastConnected) {
                    Serial.println();
                    Serial.println("disconnecting.");
                    client.stop();
                  }
                  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
                    sendData();
                  }
                  lastConnected = client.connected();
            }
        } else if(h <= 21) { // we know there is no bluetooth device
            Serial.println("No device found");
        }
        bluetooth.println("---");

    }
    clearAll();
}

void clearAll() {
    for(int i = 0; i < bluetooth.available(); i++) {
        bluetooth.read();
    }
}

// a bit hacked...
char* readSerial(int h) {
    char input;
    char buffer[100];
    if (bluetooth.available() > 0) {
        int j = 0;
        for (int i = 0; i < h; i++) { // the id's start at char 21, se we copy from there
            input = (char)bluetooth.read();
            if(i >= 21) {
                buffer[j] = input;
                buffer[j+1] = '';
                j++;
            }
        }
        return buffer;
    } else {
        return "No Data";
    }
}

void sendData() {
  Serial.println("----------------->");
  Serial.println("Trying to send card data to server for card " + btid + "...");

  // if you get a connection, report back via serial:
  // change port info as needed, this was used with a local instance via Heroku command line
  if (client.connect(server, 3001)) {
  
    Serial.println("Connected to server...");
    String feedData = "n{\"mobiledevice\" : {\"btid\" : \"" + btid + "\"}}";

    Serial.println("Sending: " + feedData + "...");
    
    client.println("PUT /checkin/ HTTP/1.0");

    client.println("Host: 192.168.1.103");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(feedData.length()));
    client.print("Connection: close");
    client.println();
    client.print(feedData);
    client.println();    

    Serial.println("Data has been sent...");
    Serial.println("----------------->");

    delay(2000); 
    
    lastConnectionTime = millis();
        
    while (client.available() && status == WL_CONNECTED) {
        if (incomingData == false)
        {
          Serial.println();
          Serial.println("--- Incoming data Start ---");
          incomingData = true;
        }
        char c = client.read();
        Serial.write(c);
        client.flush();
        client.stop();
    }

    if (incomingData == true) {
      Serial.println("--- Incoming data End");
      incomingData = false; 
    }
    if (status == WL_CONNECTED) {
      Serial.println("--- WiFi connected");
    }
    else {
      Serial.println("--- WiFi not connected");
    }
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
  
  // Reset Bluetooth ID
  btid = "";
}

void printWifiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.println("------------------");
  Serial.println("WiFi Status...");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal Strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("------------------");
  Serial.println("");
}/*
Code sample based on the following resources
- Bluetooth Detection http://www.fritz-hut.com/2012/08/26/bluetooth-presence-detection/
- WiFi http://arduino.cc/en/Tutorial/WiFiWebClientRepeating
- HTTP PUT https://www.johnbrunswick.com/2014/01/internet-of-customers-arduino-rfid-http-put-node-js/
 */
#include <SoftwareSerial.h>
#include <SPI.h>
#include <WiFi.h>
#include <string.h>

char ssid[] = "xxxxx"; // your network SSID (name) 
char key[] = "xxxxx"; // your network key

// Keep track of web connectivity
boolean lastConnected = false;
boolean incomingData = false;
int status = WL_IDLE_STATUS; // the Wifi radio's status
int keyIndex = 0; // your network key Index number

// Take care of advoiding duplicate posts
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 10*1000;

// Removing duplicate posts to Node.js service within certain interval
unsigned long lastReadTime = 0;
const unsigned long pollingInterval = 10*1000;

IPAddress server(192,168,1,103);  // numeric IP (no DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;
unsigned long lastTime;
unsigned long interval = 10000;

int bluetoothTx = 2;  // TX-O pin of bluetooth mate, Arduino D2
int bluetoothRx = 3;  // RX-I pin of bluetooth mate, Arduino D3

SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);

String btid;

void setup()
{
  Serial.begin(9600);  // Begin the serial monitor at 9600bps

  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");  // Print three times individually
  bluetooth.print("$");
  bluetooth.print("$");  // Enter command mode
  delay(100);  // Short delay, wait for the Mate to send back CMD
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  // 115200 can be too fast at times for NewSoftSerial to relay the data reliably
  bluetooth.begin(9600);  // Start bluetooth serial at 9600
  
  Serial.println("------------------");
  Serial.println("Starting - RFID and WiFi POC Test");

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, keyIndex, key);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi...");
  Serial.println("------------------");
  Serial.println("");

  printWifiStatus();
}

void loop()
{
    // Get current millis();
    unsigned long currentTime = millis();
    // Check if its already time for our process
    if((currentTime - lastTime) > interval) {
        lastTime = currentTime;
        // Enter command mode
        bluetooth.print("$$$");
                //Wait a bit for the module to respond
        delay(200);
                // Start the inquiry scan
        bluetooth.println("IN,2");
        delay(5000);
        delay(500);
        int h = bluetooth.available();
        if(h >= 42) { // we know there is a bluetooth device
            Serial.println("Someone is near...");
            char *id = readSerial(h);
            char *str;
            
            while((str = strtok_r(id, "n", &id)) != NULL) {
                    Serial.println(str);
                      
                      // TODO - find better way to deal with null at end of str
                      // variable to assign for later use as string
                      btid = (String)str;
                      btid = btid.substring(0, 19);

                  while (client.available()) {
                    char c = client.read();
                    Serial.write(c);
                  }
                  if (!client.connected() && lastConnected) {
                    Serial.println();
                    Serial.println("disconnecting.");
                    client.stop();
                  }
                  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
                    sendData();
                  }
                  lastConnected = client.connected();
            }
        } else if(h <= 21) { // we know there is no bluetooth device
            Serial.println("No device found");
        }
        bluetooth.println("---");

    }
    clearAll();
}

void clearAll() {
    for(int i = 0; i < bluetooth.available(); i++) {
        bluetooth.read();
    }
}

// a bit hacked...
char* readSerial(int h) {
    char input;
    char buffer[100];
    if (bluetooth.available() > 0) {
        int j = 0;
        for (int i = 0; i < h; i++) { // the id's start at char 21, se we copy from there
            input = (char)bluetooth.read();
            if(i >= 21) {
                buffer[j] = input;
                buffer[j+1] = '';
                j++;
            }
        }
        return buffer;
    } else {
        return "No Data";
    }
}

void sendData() {
  Serial.println("----------------->");
  Serial.println("Trying to send card data to server for card " + btid + "...");

  // if you get a connection, report back via serial:
  // change port info as needed, this was used with a local instance via Heroku command line
  if (client.connect(server, 3001)) {
  
    Serial.println("Connected to server...");
    String feedData = "n{\"mobiledevice\" : {\"btid\" : \"" + btid + "\"}}";

    Serial.println("Sending: " + feedData + "...");
    
    client.println("PUT /checkin/ HTTP/1.0");

    client.println("Host: 192.168.1.103");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(feedData.length()));
    client.print("Connection: close");
    client.println();
    client.print(feedData);
    client.println();    

    Serial.println("Data has been sent...");
    Serial.println("----------------->");

    delay(2000); 
    
    lastConnectionTime = millis();
        
    while (client.available() && status == WL_CONNECTED) {
        if (incomingData == false)
        {
          Serial.println();
          Serial.println("--- Incoming data Start ---");
          incomingData = true;
        }
        char c = client.read();
        Serial.write(c);
        client.flush();
        client.stop();
    }

    if (incomingData == true) {
      Serial.println("--- Incoming data End");
      incomingData = false; 
    }
    if (status == WL_CONNECTED) {
      Serial.println("--- WiFi connected");
    }
    else {
      Serial.println("--- WiFi not connected");
    }
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
    Serial.println("disconnecting.");
    client.stop();
  }
  
  // Reset Bluetooth ID
  btid = "";
}

void printWifiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.println("------------------");
  Serial.println("WiFi Status...");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal Strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("------------------");
  Serial.println("");
}