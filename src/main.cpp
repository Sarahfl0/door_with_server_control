/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-door-lock-system-using-password
 */

#include <Keypad.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "BLE.h"


#define RELAY_PIN   14 // ESP32 pin GIOP14 connected to the IN pin of relay
#define ROW_NUM     4  // four rows
#define COLUMN_NUM  4  // four columns
#define DOORTIME 30000	  // the time door remains open after open command
#define CLIENTID "nhjf213786542fnjjhfuhawiuhu"
#define MAX_MSG_SIZE 100
#define MAX_OTP_SIZE 20

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {21, 19, 18, 5}; // GIOP21, GIOP19, GIOP18, GIOP5  connect to the row pins
byte pin_column[COLUMN_NUM] = {17, 16, 4, 0};   // GIOP17, GIOP16, GIOP4, GIOP0 connect to the column pins
// GIOP16, GIOP4, GIOP0, GIOP2 connect to the column pins
char *subscription_message;
uint8_t subscription_message_length = 0;

WiFiClient client;
PubSubClient mqtt(client);
BLE myBLE;
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

const String password_1 = "1234"; // change your password here
const String password_2 = "5678";  // change your password here
const String password_3 = "545423";  // change your password here
String input_password;
unsigned long previousMillis = 0;


String wifiSSID = "Syed's Residence";
String wifiPassword = "11121718";
String mqttBroker = "broker.hivemq.com";
unsigned long wifi_interval = 30000;
const char *topic_to_publish = "DMA/door_gateway";
// const char* topic_to_publish = "DMA/door_server";
const char *topic_to_subscribe = "DMA/door_server";
char *mqtt_command;
bool keypad_input = false;
bool new_message = 0;
char msg[MAX_MSG_SIZE];
String input_otp;
// char input_otp[MAX_OTP_SIZE];

int otp_length = 4;
int key_count = 0;

void connectWifi();
void connect_mqtt();
void on_message(char *topic, byte *payload, unsigned int length);
void mqtt_reconnect();
void clear_input_otp();
void wifi_reconnect();
void executeCommand(char *command);


void setup() {
  Serial.begin(9600);
  input_password.reserve(32); // maximum input characters is 32
  connectWifi();
	// connect_mqtt();
	mqtt.setServer(mqttBroker.c_str(), 1883);
	mqtt.setBufferSize(512);
	mqtt.setCallback(on_message); // Initialize the callback routine

  pinMode(RELAY_PIN, OUTPUT); 
  digitalWrite(RELAY_PIN, HIGH); // lock the door
}

void loop() {
  	if (!mqtt.connected())
	{
		connect_mqtt();
		Serial.println("MQTT Connected");
		mqtt.publish(topic_to_publish, "ESP 32 Online!");
	}
  char key = keypad.getKey();

		if (key) 
		{
			Serial.println(key);
			if (key == '*') 
			{
				clear_input_otp();
				// reset the input password variable
			}
			else if ((key_count+1) == otp_length) {
				input_otp += key;
				key_count = key_count+1 ;
				Serial.print("input_otp given: ");
				Serial.println(input_otp);
				mqtt.publish(topic_to_publish,input_otp.c_str());
				keypad_input = false;
				clear_input_otp();

				// if the length of the given otp matches the declared otp length, send the otp to the server
			} 
			else 
			{
				if (key >= '0' && key <= '9')
				{
					input_otp += key;
					key_count = key_count+1 ;
					Serial.print("key_count: ");
					Serial.println(key_count);
					Serial.print("input_otp: ");
					Serial.println(input_otp);
										
				}
				// append new character to input password string
			}
		}
  if (new_message)
	{
		new_message = false;
		Serial.print("sub message: ");
		Serial.println(msg);
		Serial.println("****************");
		executeCommand(msg);
		msg[0] = '\0';
		subscription_message_length = 0;
	}

	mqtt.loop();
}

void connectWifi()
{
	Serial.println("Connecting To Wifi");
	WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(500);
	}

	Serial.println("Wifi Connected");
	Serial.println(WiFi.SSID());
	Serial.println(WiFi.RSSI());
	Serial.println(WiFi.macAddress());
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.gatewayIP());
	Serial.println(WiFi.dnsIP());
}
void connect_mqtt()
{
	while (!mqtt.connected())
	{
		Serial.println("Connecting with MQTT...");
		if (mqtt.connect(CLIENTID))
		{
			mqtt.subscribe(topic_to_subscribe);
			// mqtt.subscribe("workforce/pub");
		}
  }
}
void clear_input_otp()
{
		input_otp = "";
		key_count = 0;
		input_otp[0] = '\0';
}
void wifi_reconnect()
{
	unsigned long currentMillis = millis();
	// if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
	if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= wifi_interval))
	{
		Serial.print(millis());
		Serial.println("Reconnecting to WiFi...");
		WiFi.disconnect();
		WiFi.reconnect();
		previousMillis = currentMillis;
	}
}
void on_message(char *topic, byte *payload, unsigned int length)
{
	Serial.print("Message arrived on topic: ");
	Serial.print(topic_to_subscribe);
	Serial.print(". Message: ");

	subscription_message_length = length;
	// subscription_message = (char*)payload;
	// subscription_message + subscription_message_length = '\0';
	// msg[subscription_message_length] = '\0';
	for (int i = 0; i < length; i++)
	{
		msg[i] = (char)(*(payload + i));
	}
	msg[length] = '\0';

	Serial.print(msg);
	new_message = true;
	Serial.print(". Length: ");
	Serial.println(subscription_message_length);
}
void executeCommand(char *command)
{
	char keypad_term[8] = {'\0'};
	strncpy(keypad_term, command, 6);
	if (!strcmp(command, "Open"))
	{
		Serial.println("The door is opening");

		// door_open();
	}
	else if (!strcmp(command, "Close"))
	{
		Serial.println("The door is closing");
		// door_close();
	}
	else if (!strcmp(keypad_term, "Keypad"))
	{
		char length[MAX_OTP_SIZE];
		strncpy(length, command + 7, sizeof(command));
		otp_length = atoi(length);
		keypad_input = true;
		Serial.print("OTP length: ");
		Serial.println(otp_length);
	}
	else
	{
		Serial.println("Invalid Command");
	}
}