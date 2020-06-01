
/*
  Web client

  This sketch connects to a website (http://www.google.com)
  using the WiFi module.

  This example is written for a network using WPA encryption. For
  WEP or WPA, change the Wifi.begin() call accordingly.

  This example is written for a network using WPA encryption. For
  WEP or WPA, change the Wifi.begin() call accordingly.

  Circuit:
   Board with NINA module (Arduino MKR WiFi 1010, MKR VIDOR 4000 and UNO WiFi Rev.2)

  created 13 July 2010
  by dlf (Metodo2 srl)
  modified 31 May 2012
  by Tom Igoe
*/
//parte urto
/*IMU description
  /*
   The LSM9DS1 is a versatile 9DOF sensor. It has a built-in
  accelerometer, gyroscope, and magnetometer.
  It functions over either I2C (and SPI).

  Demo the following:
  How to create a LSM9DS1 object
  How to use the begin() function of the LSM9DS1 class.
  How to read the gyroscope, accelerometer, and magnetometer
  How to calculate actual acceleration, rotation speed, and magnetic field strength

  The LSM9DS1 has a maximum voltage of 3.6V. Make sure you power it
  off the 3.3V rail
*/
/* SD description
  /*
  SD card datalogger

  This example shows how to log data from three analog sensors
  to an SD card using the SD library.

  The circuit:
   analog sensors on analog ins 0, 1, and 2
   SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4 (for MKRZero SD: SDCARD_SS_PIN)

*/

/*TIME

 Udp NTP Client

 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe

 This code is in the public domain.

 */


#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h> //(+) 
#include <WiFiUdp.h>

// IMU libraries
#include <Wire.h>
#include "SparkFunLSM9DS1.h"

// SD library
#include <SD.h>



#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)


char serverAddress[] = "test.atmosphere.tools";  // server address
int port = 80;
//(port 80 is default for HTTP):

WiFiClient wifi;

unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;


//inizio dati post token
HttpClient httpClient = HttpClient(wifi, serverAddress, port);
const size_t CAPACITY = JSON_OBJECT_SIZE(2);
StaticJsonDocument<CAPACITY> jsonBuffer;
JsonObject login = jsonBuffer.to<JsonObject>();

int status = WL_IDLE_STATUS;//Serve!

int isConnected = 0;
int verifyConnection = 0;
double secondsStart = 0;

//inizio dati post urto
// allocate the memory for the document
const size_t MEASURE = JSON_OBJECT_SIZE(300);//boh per futura implementazione
StaticJsonDocument<MEASURE> doc;
// create an object
JsonObject measureObject = doc.to<JsonObject>();




bool arrayComplete = false; //mi dice se è stato inizializzato almeno una volta l'array

//imu
byte updateflag;    // print on Serial if there was an impact

// Var IMU(+)

// LSM9DS1 Library Init
LSM9DS1 imu;

//I2C addresses:
#define LSM9DS1_M  0x1E
#define LSM9DS1_AG  0x6B

#define PRINT_SPEED 250

// Earth's magnetic field varies by location. Add or subtract
// a declination to get a more accurate heading. Calculate
// your's here:
// http://www.ngdc.noaa.gov/geomag-web/#declination
#define DECLINATION 2.12

// Var SD(+)
const int chipSelect = 4;

float xaxis = 0, yaxis = 0, zaxis = 0;
float deltx = 0, delty = 0, deltz = 0;
float vibration = 0, magnitude = 0;
float sensitivity = 2; // valore da comparare ad urto; sopra questo numero avvio routine urto (traduci)
float devibrate = 75;// waiting counter to avoid false impact

double angleXY;   // X angle in XY plane in the range -180 + 180
double angleYZ;   // Y angle in YZ plane in the range -180 + 180
double angleXZ;   // X angle in XZ plane in the range -180 + 180
unsigned long time1; //Time when the program started. Used to run impact routine every 2mS
unsigned long time2;

File file;
char temporaneo = 'b'; //scelto a caso
String stringaDai = "";
int contatore = 1; //serve per inserire le variabili salvate sulla scheda sd al posto giusto
String tempImpactTime = "";
String tempMagnitude = "";//verranno utilizzati come variabili temporanee per prendere il valore e inviarli con la post(verranno inserite come input alla funzione della post)
String tempAngleXY = "";
String tempAngleYZ = "";
String tempAngleXZ = "";


int dataPrintedSD = 0; //sentinella per vedere quanti dati ho salvati sulla scheda sd in caso di mancanza di connessione
double impactTime = 0;
int impactDetected = 0;

#define seconds()(millis()/1000)

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  firstConnection();//connessione al wifi

  // IMU(+)
  Serial.begin(115200);
  imuSettings();


  String tokenLogin = getToken();
  secondsStart = getSecondsServer();
}




void loop() {
  impactDetected = impactAnalysis(); //impactDetected 1 se riscontra impatto , 0 se non c'è
  if (impactDetected == 1) //impatto avvenuto
  {
    impactDetected = 0; //azzero(in realtà non serve)
    isConnected = connection();
    if (isConnected == 0) { //se connesso a internet invio direttamente il dato
      postOnlineData(0);
    }
    else { //se non connesso lo salvo su scheda sd
      printSD();
      dataPrintedSD++;//ho scritto 1 riga sulla scheda sd
      Serial.print("Righe salvate su SD = "); Serial.println(dataPrintedSD);
    }
  }
  if (verifyConnection > 5000) {
    verifyConnection = 0;
    isConnected = connection(); //ho un aggiornamento periodico per sapere se c'è connessione o no
    //manca il codice che chiede alla sentinella dataPrintedSD se è diversa da 0, in quel caso devo riprendere i valori sulla scheda SD nel caso
    // io abbia connessione e inviarli al server con la post
    if (WiFi.status() == 3 && dataPrintedSD != 0) {
      readSDandPost();
    }

  }
  verifyConnection++;

  delay(1);//do un millisecondo di delay

}//fine loop









void firstConnection() {//first perchè è la prima nel setup che verifica tutto (diamo per scontato che abbia successo)
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  //if (WiFi.status() == WL_NO_MODULE) {
  // Serial.println("Communication with WiFi module failed!");
  // don't continue
  //while (true);
  // }

  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (WiFi.status() != 3) {//3==WL_CONNECTED
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(5000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();
  //fine parte login al wifi del telefono o modem(+)
}



void imuSettings() {
  // Before initializing the IMU, there are a few settings we may need to adjust.
  // Use the settings struct to set the device's communication mode and addresses:
  imu.settings.device.commInterface = IMU_MODE_I2C;
  imu.settings.device.mAddress = LSM9DS1_M;
  imu.settings.device.agAddress = LSM9DS1_AG;

  imu.settings.accel.scale = 16; // Set accel range to +/-16g
  imu.settings.gyro.scale = 2000; // Set gyro range to +/-2000dps
  imu.settings.mag.scale = 8; // Set mag range to +/-8Gs

  imu.begin(); // Call begin to update the sensor's new settings

  // The above lines will only take effect AFTER calling imu.begin(), which verifies
  // communication with the IMU and turns it on.
  if (!imu.begin())
  {
    Serial.println("Failed to communicate with LSM9DS1.");
    while (1);
  }
  time1 = millis();   // millisecondi passati da avvio Arduino(traduci)

  // SD(+)
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
}



int connection() {//false se non c'è connessione, true se c'è

  // if (WiFi.status() == WL_NO_MODULE) {
  //  Serial.println("Communication with WiFi module failed!");//verifica se per caso il modulo wifi si è scollegato o rotto
  //  return 1;//false
  // }

  // attempt to connect to Wifi network:
  if (WiFi.status() != 3) {//3 == WL_CONNECTED
    Serial.print("Attempting to connect to SSID: ");//se non c'è connessione tenta di farlo una volta sola
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    if (WiFi.status() != 3) {//se non riesce falso
      Serial.println("No wifi, using SD memory");
      return 1;//false
    }
  }
  Serial.println("Connected to wifi");
  return 0;//true connessione avvenuta
}



String getToken() {
  login["username"] = "impact-sensor-user-username";
  login["password"] = "impact-sensor-user-password";

  // if you get a connection, report back via serial:
  if (wifi.connect(serverAddress, port)) {
    Serial.println("making POST request");
    postLoginFunction(login);

  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
  // read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();//è il token di ritorno

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  return response;
}



double getSecondsServer() {//da modificare con la get al server
Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  if (Udp.parsePacket()) {
    Serial.println("packet received");
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    double secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);
  double secondsSince1970=(secsSince1900-2208988800);//è il valore per passare dal 1900 al 1970
   Serial.print("seconds since Jan 1 1970 = ");
    Serial.println(secondsSince1970);
    
  //double milliseconds = 1560081600000;//modificato in data 06/09
  return secondsSince1970;//va anche sottratto millis() del momento sennò errore nei tempi
}
else {getSecondsServer();}
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}



void postLoginFunction(JsonObject& data) {
  String dataStr = "";
  serializeJson(data, dataStr);
  httpClient.beginRequest();
  httpClient.post("/v1/login");
  httpClient.sendHeader("Content-Type", "application/json");
  httpClient.sendHeader("Content-Length", dataStr.length());
  httpClient.beginBody();
  httpClient.print(dataStr);
  httpClient.endRequest();

  wifi.flush();
}



int impactAnalysis() { //1 se impatto è avvenuto, 0 se no
  // IMU(+)
  imu.readAccel();
  imu.readGyro();
  imu.readMag();

  if (millis() - time1 > 1.99) Impact();  // call impact routine every 2mS

  if (updateflag > 0)   // an impact was detected.
  {
    updateflag = 0;   // flag reset
    Serial.print("Impact detected!!\tMagnitude:"); Serial.print(magnitude);
    Serial.print("\t AngleXY:"); Serial.print(angleXY, 0); Serial.print("\t AngleYZ:"); Serial.print(angleYZ, 0); Serial.print("\t AngleXZ:"); Serial.print(angleXZ, 0);
    Serial.println("\n\n");
    return 1;
  }
  return 0;
}



void postOnlineData(int value) {
  String token = getToken();
  //ora parte seconda post
  measureObject["thing"] = "product";
  measureObject["feature"] = "impact";
  measureObject["device"] = "impact-sensor";
  //if (!arrayComplete)
  //{
  if (value == 0) {
    firstCompleteNested();//nel caso in cui all'impatto ho connessione
  }
  if (value == 1)
  {
    SDCompleteNested();
  }

  //arrayComplete=true;
  //}
  //else
  //{
  //  setCompleteNested();
  //}

  postMeasureFunction(measureObject, token);

  int statusCode2 = httpClient.responseStatusCode();
  String response2 = httpClient.responseBody();//è il body di ritorno

  Serial.print("Status code2: ");
  Serial.println(statusCode2);
  Serial.print("Response: ");
  Serial.println(response2);
}



void postMeasureFunction(JsonObject& measureData, String token) {
  String dataStr2 = "";
  serializeJson(measureData, dataStr2);
  httpClient.beginRequest();
  httpClient.post("/v1/measurements");
  httpClient.sendHeader("Content-Type", "application/json");
  httpClient.sendHeader("Content-Length", dataStr2.length());
  httpClient.sendHeader("Authorization", token);//da fare quando ricevi il token per inviare i successivi dati
  httpClient.beginBody();
  httpClient.print(dataStr2);
  httpClient.endRequest();

  wifi.flush();
}



void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}



void firstCompleteNested()
{
  //devo innestare un array di un oggetto di un array
  JsonArray values = measureObject.createNestedArray("values");

  // allocate the memory for the document
  const size_t MEASURE2 = JSON_OBJECT_SIZE(15);//boh per futura implementazione
  StaticJsonDocument<MEASURE2> doc2;
  // create an object
  JsonObject measureObject2 = doc2.to<JsonObject>();

  JsonArray value = measureObject2.createNestedArray("value");
  // add some values
  value.add(String(impactTime, 0));
  value.add(String(magnitude, 0));
  value.add(String(angleXY, 0));
  value.add(String(angleYZ, 0));
  value.add(String(angleXZ, 0));
  values.add(measureObject2) ;
  serializeJson(measureObject, Serial);
}



void SDCompleteNested()
{
  //devo innestare un array di un oggetto di un array
  JsonArray values = measureObject.createNestedArray("values");

  // allocate the memory for the document
  const size_t MEASURE2 = JSON_OBJECT_SIZE(15);//boh per futura implementazione
  StaticJsonDocument<MEASURE2> doc2;
  // create an object
  JsonObject measureObject2 = doc2.to<JsonObject>();

  JsonArray value = measureObject2.createNestedArray("value");
  // add some values
  value.add(tempImpactTime);
  value.add(tempMagnitude);
  value.add(tempAngleXY);
  value.add(tempAngleYZ);
  value.add(tempAngleXZ);
  values.add(measureObject2) ;
  serializeJson(measureObject, Serial);
}



/*void setCompleteNested()
  {

  // compute the required size
  const size_t CAPACITY = JSON_ARRAY_SIZE(5);

  // allocate the memory for the document
  StaticJsonDocument<CAPACITY> doc;

  // create an empty array
  JsonArray arrayDati = doc.to<JsonArray>();

  // add some values
  arrayDati.add(String(impactTime,0));
  arrayDati.add(String(magnitude,0));
  arrayDati.add(String(angleXY,0));
  arrayDati.add(String(angleYZ,0));
  arrayDati.add(String(angleXZ,0));
  value.remove(4);
  value.remove(3);
  value.remove(2);
  value.remove(1);
  value.remove(0);
  // serialize the array and send the result to Serial
  //serializeJson(doc, Serial);
  // String arrayDati[]={String(impactTime,0),String(magnitude,0),String(angleXY,0),String(angleYZ,0),String(angleXZ,0)};
  value.set(arrayDati);
  // add some values
  //value.set(0,String(impactTime, 0));
  // value.set(1,String(&magnitude, 0));
  // value.set(2,String(&angleXY, 0));
  // value.set(3,String(&angleYZ, 0));
  // value.set(4,String(&angleXZ, 0));
  //values.set(0,measureObject2) ;
  values.remove(0);
  values.add(measureObject2) ;
  serializeJson(measureObject, Serial);
  }
*/

void Impact()
{
  /*
    //Calibration testing. Accelerometer is calibrated correctly x ~0 y ~0 z ~9.8
    Serial.print("X: "); Serial.print(event.acceleration.x); Serial.print(" ");
    Serial.print("Y: "); Serial.print(event.acceleration.y); Serial.print(" ");
    Serial.print("Z: "); Serial.print(event.acceleration.z); Serial.print(" ");Serial.println("m/s^2 ");
  */

  time1 = millis(); // resets time value

  float oldx = xaxis; // local variables store previous axis readings for comparison
  float oldy = yaxis;
  float oldz = zaxis;

  //Serial.print("current time = "); Serial.print(millis()); Serial.print("\toldx = "); Serial.print(oldx); Serial.print("\toldy = "); Serial.print(oldy); Serial.print("\toldz = "); Serial.println(oldz);

  vibration--; // loop counter prevents false triggering. Vibration resets if there is an impact. Don't detect new changes until that "time" has passed.
  //Serial.print("Vibration = "); Serial.println(vibration);
  if (vibration < 0) vibration = 0;
  //Serial.println("Vibration Reset!");
  //Serial.println("****************************");

  xaxis = imu.calcAccel(imu.ax);
  yaxis = imu.calcAccel(imu.ay);
  zaxis = imu.calcAccel(imu.az);

  //Serial.print("current time = "); Serial.print(millis()); Serial.print("\txaxis = "); Serial.print(xaxis); Serial.print("\tyaxis = "); Serial.print(yaxis); Serial.print("\tzaxis = "); Serial.println(zaxis);
  //Serial.println("****************************");

  if (vibration > 0) return;

  deltx = xaxis - oldx;
  delty = yaxis - oldy;
  deltz = zaxis - oldz;

  magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz)); // Magnitude to calculate force of impact. Pythagorean theorem used.

  if (magnitude >= sensitivity) //impact detected
  {
    impactTime = secondsStart + seconds();

    //Values that caused the impact
    Serial.print("Impact Time = "); Serial.print(impactTime); Serial.print(" ms");
    Serial.println();
    Serial.print("oldx = "); Serial.print(oldx); Serial.print("\toldy = "); Serial.print(oldy); Serial.print("\toldz = "); Serial.println(oldz);
    Serial.print("xaxis = "); Serial.print(xaxis); Serial.print("\tyaxis = "); Serial.print(yaxis); Serial.print("\tzaxis = "); Serial.println(zaxis);
    Serial.print("Mag = "); Serial.print(magnitude);
    Serial.print("\t\t(deltx = "); Serial.print(deltx);
    Serial.print("\tdelty = "); Serial.print(delty);
    Serial.print("\tdeltz = "); Serial.print(deltz); Serial.println(")");


    updateflag = 1;

    //double X = xaxis - 512; // adjust xaxis reading to +/- 512
    //double Y = yaxis - 512; // adjust yaxis reading to +/- 512


    double X = imu.calcAccel(imu.ax);   // used a double value for a better measure
    double Y = imu.calcAccel(imu.ay);
    double Z = imu.calcAccel(imu.az);


    Serial.print("X = "); Serial.print(X); Serial.print("\tY = "); Serial.print(Y); Serial.print("\tZ = "); Serial.println(Z);

    Serial.println("****************************");

    angleXY = ((atan2(Y, X) * 180) / PI) + 180; // use atan2 to calculate angle and convert radians to degrees
    angleXY = range(angleXY);
    angleYZ = ((atan2(Z, Y) * 180) / PI) + 180; // use atan2 to calculate angle and convert radians to degrees
    angleYZ = range(angleYZ);
    angleXZ = ((atan2(Z, X) * 180) / PI) + 180; // use atan2 to calculate angle and convert radians to degrees
    angleXZ = range(angleXZ);
    //faccio + 180 e resto con 360 perchè il vettore accelerazione è opposto alla direzione dove è avvenuto l'urto

    vibration = devibrate;                                      // reset anti-vibration counter
    time2 = millis();
  }
  else
  {
    //if (magnitude > 2)
    //Serial.println(magnitude);
    magnitude = 0;                                            // reset magnitude of impact to 0
  }
}



double range(double angleValue)
{
  if (angleValue > 360)
  {
    angleValue -= 360;
  }
  if (angleValue < 0)
  {
    angleValue += 360;
  }
  return angleValue;
}


void printSD() {
  //SD(+)

  // make a string for assembling the data to log:
  String dataString = "";

  dataString = saveSDString();//stringa da salvare nel file(+)

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.println("ho salvato l'urto su scheda SD");
  }

  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}



String saveSDString()// funzione che analizza i dati(+)
{
  String dataSD = "";// inizializzo vuoto
  dataSD += String(impactTime) + " " + String(magnitude) + " " + String(angleXY, 0) + " " + String(angleYZ, 0) + " " + String(angleXZ, 0) + " ";
  return dataSD;
}


void readSDandPost()
{ //si potrebbe pensare di mettere "10 5 15 30 " ricordandosi lo spazio dopo il 30 per evitare di perdere un valore poi invio i valori e torno lì in base alla sentinella so se andare avanti o no, se no chiudo il file se si evito lo \n o \r e ricomincio
  file = SD.open("datalog.txt"); //Riapro il file
  if (file) //Se il file è stato aperto correttamente
  {
    Serial.println("Contenuto file datalog.txt: ");
    //Continua fino a che c'è qualcosa
    while (file.available())
    {
      temporaneo = (file.read());
      if (temporaneo != '\n' && temporaneo != '\t' && temporaneo != '\r')//le ignoro, sicuramente tra una riga di dati c'è un simbolo che fa andare a capo ma per come è scritto il codice lo ignora e comincia la seconda riga come fosse la prima che incontra
      {

        if (temporaneo != ' ') {
          // Serial.print("temporaneo è:");
          //Serial.print(temporaneo);
          stringaDai += temporaneo;
          //Serial.print("stringa è:");
          // Serial.print(stringaDai);
        }
        else {//raggiunto lo spazio le cose in ordine vanno inserite al proprio posto

          //Serial.print(stringaDai);

          if (contatore == 1)
          { tempImpactTime = stringaDai;
            contatore++;
            Serial.print(tempImpactTime + " inserito in tempo impatto; ");
          }
          else if (contatore == 2)
          { tempMagnitude = stringaDai;
            contatore++;
            Serial.print(tempMagnitude + " inserito in magnitudo; ");
          }
          else if (contatore == 3)
          { tempAngleXY = stringaDai;
            contatore++;
            Serial.print(tempAngleXY + " inserito in angolo XY; ");
          }
          else if (contatore == 4)
          { tempAngleYZ = stringaDai;
            contatore++;
            Serial.print(tempAngleYZ + " inserito in angolo YZ; ");
          }
          else //contatore==5
          { tempAngleXZ = stringaDai;
            contatore = 1;
            Serial.println(tempAngleXZ + " inserito in angolo XZ; ");

            postOnlineData(1);//qui andrebbe inserita la post e da ora tutti gli elementi temporanei diventano riscrivibili
            dataPrintedSD--;//tolgo la riga e vado avanti
          }
          stringaDai = "";//rendo di nuovo vuota la stringa
        }
      }
    }

    file.close(); //Chiudo file
    SD.remove("datalog.txt");// qui devo ANCHE cancellare il file perchè significa che ho inviato tutto e il file non ha più elementi utili
    Serial.println("ho cancellato il file tutti gli elementi inviati");
    if (dataPrintedSD != 0) {
      Serial.print("il valore di dataPrintedSD non è 0 potrebbe esserci un errore");
      dataPrintedSD = 0;
    }
  }

  else
  {
    Serial.println("ERRORE: apertura file datalog.txt");
  }
}
