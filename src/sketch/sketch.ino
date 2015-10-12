#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>

#include <LiquidCrystal.h>
#include <Time.h>

#define DURATION 60
#define SENSOR_PIN 0
#define LED_PIN 13

LiquidCrystal lcd(9, 8, 5, 4, 3, 2);
byte mac[] = {0x90, 0xA2, 0xDA, 0x0D, 0x14, 0x90};
const unsigned int ntpLocalPort = 8888;
const char timeServerName[] = "pool.ntp.org";
IPAddress timeServer;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
EthernetUDP Udp;
unsigned long lastSendPacketTime = 0;
int d = 0;
unsigned long s = 0;

void setup()
{
	pinMode(LED_PIN, OUTPUT);

	lcd.begin(16, 2);
	lcd.clear();

	while (Ethernet.begin(mac) != 1) {
		lcd.setCursor(0, 1);
		lcd.print("DHCP Failed.");
		delay(1000);
	}

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("IP address: ");
	lcd.setCursor(0, 1);
	lcd.print(Ethernet.localIP());
	
	delay(1000);
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Gateway: ");
	lcd.setCursor(0, 1);
	lcd.println(Ethernet.gatewayIP());
	
	delay(1000);
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("DNS address: ");
	lcd.setCursor(0, 1);
	lcd.print(Ethernet.dnsServerIP());

	delay(1000);
	Udp.begin(ntpLocalPort);
	sendNTPpacket(timeServerName);
	lastSendPacketTime = millis();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Calculating...");
}


void loop()
{
	// Average value for the last 1 minute.
	s = 0;
	for (int i = 0; i < 60; i++) {
		d = analogRead(SENSOR_PIN);
		s = s + d;
		lcd.setCursor(0, 1);
		lcd.print("Now: ");
		lcd.print(calcCelsius(d));
		lcd.write(0xDF);
		lcd.print("C ");
		delay(1000);
	}
	int rawvoltage = s / 60;
	float celsius = calcCelsius(rawvoltage);

	// Display
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("Ave: ");
	lcd.print(celsius);
	lcd.write(0xDF);
	lcd.print("C ");

	// Time sync
	if (millis() - lastSendPacketTime > 86400000) {
		sendNTPpacket(timeServerName);
		lastSendPacketTime = millis();
	}
	if (Udp.parsePacket()) {
		Udp.read(packetBuffer, NTP_PACKET_SIZE);
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		const unsigned long seventyYears = 2208988800UL;
		unsigned long epoch = secsSince1900 - seventyYears;
		setTime(epoch + (9 * 60 * 60));
	}
	Ethernet.maintain();
}


float calcCelsius(int rawvoltage)
{
	float millivolts = (rawvoltage/1024.0) * 4900;
	float kelvin = (millivolts/10);
	float celsius = kelvin - 273.15;
	return celsius;
}


// send an NTP request to the time server.
unsigned long sendNTPpacket(const char* timeServer)
{
	IPAddress address;
	DNSClient dns;
	dns.begin(Ethernet.dnsServerIP());

	if(dns.getHostByName(timeServer, address)) {
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("NTP server:");
		lcd.setCursor(0, 1);
		lcd.print(address);
	}
	else {
		lcd.setCursor(0, 1);
		lcd.print("DNS failed.");
	}
	
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE); 
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49; 
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp: 		   
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer,NTP_PACKET_SIZE);
	Udp.endPacket(); 
}
