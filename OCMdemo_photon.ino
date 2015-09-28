// OCMDemo.ino

#include "HttpClient.h"
#include "SparkTime.h"
// #include <HttpClient/HttpClient.h>
// #include <SparkTime/SparkTime.h>

#define DEBUG

// Data limits

#define DATA_LIMIT 1000
#define DATA_WARNING_THRESHOLD .8
#define DATA_WARNING_LIMIT (DATA_LIMIT * DATA_WARNING_THRESHOLD)

#define DELAY_BETWEEN_POSTS 20000    // miliseconds to wait between posts.

// LED pins
#define GREEN_LED 2
#define YELLOW_LED 3
#define RED_LED 4

// Range values so that we can scale properly.

#define ACTUAL_MAX (4094.0)
#define INTENDED_MAX (1200.0)
#define RANGE_FACTOR (INTENDED_MAX / ACTUAL_MAX)

// NTP time is based on an epoch of 1-Jan-1900.  Unix time is based on an epoch of 1-Jan-1970.
// The offset is 2208988800 seconds.

#define NTP_TIME_OFFSET_IN_SECONDS 2208988800

// The RTC code apparently doesn't deal with timezones, even though it says it does.  Therefore,
// offset the value by our (GMT-7) timezone.

#define TZ_OFFSET_IN_SECONDS (-7 * 60 * 60)

int potPin = 2;
int dataVolume = 0;

// unsigned int nextTime = 0;    // Next time to contact the server
// String destination_url = "https://sapiotbcdfb1bb3.us1.hana.ondemand.com/sap/ocm/services/ocm.xsodata/usage";
// char *server = "sapiotbcdfb1bb3.us1.hana.ondemand.com";
// IPAddress server = {172,25,113,108};
IPAddress server = {54,85,170,183};
// int port = 8000;
int port = 441;
char *path = "/sap/ocm/services/ocm.xsodata/usage";
// char *path = "/";
HttpClient http;
http_request_t request;
http_response_t response;
http_header_t headers[] = {
    {"Content-Type", "application/json"},
    {"Accept" , "application/json" },
    {"Authorization", "Basic STgzMTUzMzpBYWFhMTIzNA=="},
    {NULL, NULL} // NOTE: Always terminate headers will NULL
};

// Time related

UDP UDPClient;
SparkTime rtc;

void setup() {

  Serial.begin(9600);

  // Set up the pins for the LEDs.

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Set up Wi-Fi.

  delay(5000);  // Time to conenct with serial program.

  WiFi.on();
  while (WiFi.connecting()) {
    delay(100); // Just wait for the connection.
  }
  if (!WiFi.ready()) {
    Serial.println("Wi-Fi was unable to connect.  Halting.");
    while(1);
  } else {
    // All good!
    Serial.print("Wi-Fi connected to SSID "); Serial.print(WiFi.SSID()); Serial.print(", IP addresss is ") ; Serial.println(WiFi.localIP());
  }

  // Set up time.

  rtc.begin(&UDPClient);
  rtc.setTimeZone(-8); // gmt offset
}

String toStringWithLeadingZero(unsigned int value) {
    String returnValue = "";
    if (value < 10) {
        returnValue += "0";
    }
    returnValue += String(value);
    return(returnValue);
}

// Return the time and date in the YYYY-MM-DDTHH:MM:SS
String getTimeAndDate() {
    unsigned long currentTime = rtc.now();
    String time = "";
    time += toStringWithLeadingZero(rtc.year(currentTime));
    time +=  "-";
    time += toStringWithLeadingZero(rtc.month(currentTime));
    time +=  "-";
    time += toStringWithLeadingZero(rtc.day(currentTime));
    time +=  "T";
    time += toStringWithLeadingZero(rtc.hour(currentTime));
    time +=  ":";
    time += toStringWithLeadingZero(rtc.minute(currentTime));
    time +=  ":";
    time += toStringWithLeadingZero(rtc.second(currentTime));
    return(time);
}

void postToHana(int dataVolume) {

    request.hostname = server;
    request.port = port;
    request.path = path;
    unsigned long secondsSinceEpoch = rtc.now() - NTP_TIME_OFFSET_IN_SECONDS + TZ_OFFSET_IN_SECONDS;
    // Add three zeros to the time value, which is the same thing as multiplying by 1000 to convert to milliseconds.
    String body = "{ \"TIMESTAMP\": \"/Date(" + String(secondsSinceEpoch) + String("000") + ")/\", \"USAGE\": \"" + String(dataVolume) + "\" }";
    request.body = body;
#ifdef DEBUG
    Serial.println("==================== Request body =======================");
    Serial.println(body);
    Serial.println("=================== End of request ======================");
  #endif // DEBUG

    http.post(request, response, headers);

#ifdef DEBUG
    Serial.println("=================== Response status =====================");
    Serial.print("***** POST Response status: ");
    Serial.println(response.status);
    Serial.println("==================== Response body ======================");
    Serial.println(response.body);
    Serial.println("=================== End of response =====================");
#endif // DEBUG
}

void turnon(int pin) {
  digitalWrite(pin, HIGH);
}

void turnoff(int pin) {
  digitalWrite(pin, LOW);
}

void lightGreenLED() {
  turnoff(RED_LED);
  turnoff(YELLOW_LED);
  turnon(GREEN_LED);
}

void lightYellowLED() {
  turnoff(GREEN_LED);
  turnoff(RED_LED);
  turnon(YELLOW_LED);
}

void lightRedLED() {
  turnoff(GREEN_LED);
  turnoff(YELLOW_LED);
  turnon(RED_LED);
}

void loop() {

  dataVolume = analogRead(potPin);

  // In the pot I'm using, zero is all the way to the right, and full value
  // is immediate after the switch.  Invert these so that they make more sense.
  // Note that we have to have the high range of the pot for this calculation.

  dataVolume = abs(dataVolume - ACTUAL_MAX);

  // Scale the value so that it falls within the expected range.

  dataVolume *= RANGE_FACTOR;

  // Light the correctly colored LED.  Green is lit if you are below
  // the DATA_WARNING_LIMIT.  Yellow is lit if you are between the DATA_WARNING_LIMIT
  // and the DATA_LIMIT.  Red is lit if you are at or over the DATA_LIMIT.

  if (dataVolume < DATA_WARNING_LIMIT) {
    lightGreenLED();
  } else if (dataVolume < DATA_LIMIT) {
    lightYellowLED();
  } else {
    lightRedLED();
  }

  postToHana(dataVolume);

  delay(DELAY_BETWEEN_POSTS);
}
