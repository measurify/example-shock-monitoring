# Design and development of an embedded device for impact assessment

## Overview

This project is intended to detect impacts that an object could suffer during its movement (for example, during a relocation or the shipping of an artwork).
The software, besides acquire and elaborate the data in order to decide how intese the impact was, send them to an API called [*Measurify*](https://drive.google.com/file/d/10dYphKwHbEJ-b69iEGDVpFh5caEc4vpM/view?usp=sharing) (formally known as *Atmosphere*); if there is no connection available, the record of the impacts aren't lost, but saved locally in a SD card.
Finally a web interface is created so that the user can view the data in the easiest way possible.

## Hardware

The main components:

- **Microcontroller** ([Arduino MKR WiFi 1010](https://store.arduino.cc/mkr-wifi-1010))
- **MicroSD support** ([MKR Mem Shield](https://store.arduino.cc/mkr-mem-shield))
- **IMU sensor** ([SparkFun 9DoF IMU Breakout - LSM9DS1)](https://www.sparkfun.com/products/13284)

A complete hookup guide with a brief description of the sensor can be found [here](https://learn.sparkfun.com/tutorials/lsm9ds1-breakout-hookup-guide?_ga=2.217884755.452313816.1563890620-213251003.1554896041).

![The device used during the sperimentation](images/figure1.jpg?raw=true "Figure 1")

## Software

During the development of the project, these Arduino's libraries were used:

- **ArduinoJSON**
- **ArduinoHttpClient**
- **WiFiUdp**

The first library is required as *Measurify* only accepts JSON format for its POST; the second one help the user to work with web servers directly from Arduino.
Finally, the last library is used to retrive the global time from the Internet, so that all the impacts are precisely located in time.

The code has the following behaviour:

### Setup

- First Internet connection

	```
		void firstConnection() {
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

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
	}

- Inizialize IMU's parameters

- Token's retrive from the API server

	```
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
  String response = httpClient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  return response;
	}

### Loop

- Impact analysis

	```
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

	
- Check for Internet connection
	- If connected:
		- Create a JSON file and POST
		- Wait for the response from the server
		- Back to the loop
		
	- If not connected:
		- Save the data in a txt file by adding a line for every measure
		- Back to the loop and every 5 seconds check for the connection: if connected, read every line in the txt file and send them to the server, then delete the file.







