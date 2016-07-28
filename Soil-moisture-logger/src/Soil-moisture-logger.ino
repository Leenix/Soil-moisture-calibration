#include "HX711.h"
#include "logging.h"
#include "Adafruit_ESP8266.h"
#include "ProgmemString.h"
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <SimpleTimer.h>
#include "config.h"


////////////////////////////////////////////////////////////////////////////////
// Objects

HX711 scale_1(A1, A2);
HX711 scale_2(A3, A4);
HX711 load_cells[] = {scale_1, scale_2};

SoftwareSerial wifi_bridge(WIFI_RX, WIFI_TX);
Adafruit_ESP8266 wifi(&wifi_bridge, &Serial);

SimpleTimer timer;

SensorData data;

char packet_buffer[PACKET_BUFFER_SIZE];
char date_buffer[DATESTRING_SIZE];

////////////////////////////////////////////////////////////////////////////////
// Main Functions

void setup(){
    /**
    * Initialise the calibration test.
    */
    Log.Init(LOG_LEVEL_VERBOSE, 57600);
    Log.Info(P("Soil moisture calibration logger"));

    start_hygrometer();
    start_soil_forks();
    start_load_cells();
    //start_wifi();
}


void loop(){
    /**
    * Main loop - check the timer.
    * All functions fire from callback events
    */
    timer.run();
}


//////////////////////////////////////////////////////////////////////////
// Gypsum block sensors (homemade)

void start_hygrometer(){
	/**
	* Start the gypsum sensors
	*/
	pinMode(HYGROMETER_CONTROL[LEFT], OUTPUT);
	pinMode(HYGROMETER_CONTROL[RIGHT], OUTPUT);

	hygrometer_off();

	update_gypsum_sensors();
	timer.setInterval(SAMPLE_INTERVAL, update_gypsum_sensors);
    Log.Debug(P("Hygrometers recording"));
}


void hygrometer_off(){
	/**
	* Cut power to the hygrometer(s)
    * All hygrometers currently share the same power/enable pins.
    * Driving both of these pins low will disable all attached hygrometers.
	*/
	digitalWrite(HYGROMETER_CONTROL[LEFT], LOW);
	digitalWrite(HYGROMETER_CONTROL[RIGHT], LOW);
}


int get_gypsum_moisture(int sensor_number){
	/**
	* Measure the soil saturation using the gypsum hygrometer.
	* The hygrometers are measured in both directions to minimise galvanic
	* corrosion.
	*
	* @param sensor_number ID of gypsum sensor to measure {0|1}
	* @ret Soil saturation measured by the hygrometer in %
	*/
	sensor_number = constrain(sensor_number, 0, NUM_GYPSUM_SENSORS);

	// Measure resistance through left side
	hygrometer_off();
	digitalWrite(HYGROMETER_CONTROL[LEFT], HIGH);
    delay(1000);
	unsigned int hygrometer_left = analogRead(HYGROMETER_LEFT[sensor_number]);
    Log.Verbose(P("Hygrometer left: %d"), hygrometer_left);

	// Measure resistance through right side to reduce galvanic corrosion
	hygrometer_off();
	digitalWrite(HYGROMETER_CONTROL[RIGHT], HIGH);
    delay(1000);
	unsigned int hygrometer_right = analogRead(HYGROMETER_RIGHT[sensor_number]);
    Log.Verbose(P("Hygrometer right: %d"), hygrometer_right);

    hygrometer_off();

    unsigned int hygrometer_reading = (hygrometer_left + hygrometer_right)/2;

    char value[8];
    float gypsum_voltage = hygrometer_left*(INPUT_VOLTAGE)/1024;
    unsigned long hygrometer_resistance = (HYGROMETER_RESISTANCE * (INPUT_VOLTAGE- DIODE_FORWARD_VOLTAGE))/gypsum_voltage - HYGROMETER_RESISTANCE;
    dtostrf(gypsum_voltage, 0, 3, value);
    Log.Verbose(P("Gypsum voltage:\t%sV"), value);

    Log.Debug(P("Hygrometer %d reading: %d"), sensor_number, hygrometer_reading);
	return hygrometer_reading;
}


void update_gypsum_sensors(){
	/**
	* Read the gypsum moisture levels into the data object
	*/
	for (size_t i = 0; i < NUM_GYPSUM_SENSORS; i++) {
		data.gypsum[i] = get_gypsum_moisture(i);
	}
}


//////////////////////////////////////////////////////////////////////////
// Soil Moisture (DFRobot Moisture Sensor (Fork))

void start_soil_forks(){
	/**
	* Initialise the soil moisture sensor
    * Regular sensor readings are called by a timer event, which is also set up in this function.
	*/
	pinMode(SOIL_FORK_1, INPUT);
	pinMode(SOIL_FORK_2, INPUT);

	timer.setInterval(SAMPLE_INTERVAL, update_soil_fork_moisture);
    Log.Debug(P("Soil forks recording"));
}


int get_soil_fork_moisture(int num_fork){
	/**
	* Read the soil moisture from the soil fork
	* @param num_fork Soil fork number (0/1)
	* @ret Soil moisture as an arbitrary 10-bit count
	*/
	num_fork = constrain(num_fork, 0, 1);
	int soil_fork_reading = analogRead(SOIL_FORKS[num_fork]);
    Log.Debug(P("Soil fork %d reading: %d"), num_fork, soil_fork_reading);

    return soil_fork_reading;
}


void update_soil_fork_moisture(){
	/**
	* Read surface soil moisture level into the data object
    * Timer callback function
	*/
    for (int i = 0; i < NUM_SOIL_FORKS; i++) {
        data.soil_fork[i] = get_soil_fork_moisture(i);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Load cells

void start_load_cells(){
    for (int i = 0; i < NUM_LOAD_CELLS; i++) {
        load_cells[i].power_up();
        load_cells[i].set_scale(LOAD_CELL_CALIBRATION_FACTOR);
    }

    timer.setInterval(SAMPLE_INTERVAL, update_load_cells);
}


void update_load_cells(){
    for (int i = 0; i < NUM_LOAD_CELLS; i++) {
        data.mass[i] = get_mass(i);
    }
}


float get_mass(int load_cell_number){
    float mass =  load_cells[load_cell_number].get_units(NUM_LOAD_CELLS_READS);

    char value[8];
    dtostrf(mass, 0, 1, value);
    Log.Debug(P("Load cell %d mass: %s grams"), load_cell_number, value);

    return mass;
}


//////////////////////////////////////////////////////////////////////////
// WiFi

void start_wifi(){
	Log.Info(P("Starting WiFi..."));
    wifi.setBootMarker(F("Version:0.9.2.4]\r\n\r\nready"));
	wifi_bridge.begin(WIFI_BAUD);
    wifi.softReset();
    delay(50);

    Log.Debug(P("Connecting to %s..."), WIFI_SSID);
	if (wifi.connectToAP(F(WIFI_SSID), F(WIFI_PASS))){
        timer.setInterval(UPLOAD_INTERVAL, upload_packet);
        Log.Info(P("Connected to %s"), WIFI_SSID);
	}else{
		Log.Error(P("Could not connect to WiFi"));
	}
}


void upload_packet(){
	/**
	* Send the data packet to Dweet.io
	*
	* @param out_message
    * @param message_size
	*/
    assemble_data_packet(packet_buffer);

    int connection_attempts = 0;
    bool tcp_connected = false;
    bool success = false;

    // Attempt to connect to the upload server.
    // The library kind of sucks at knowing if it's successful or not, so just attempt the GET afterwards anyway...
    while (!tcp_connected && connection_attempts < MAX_CONNECT_ATTEMPTS){
        tcp_connected = wifi.connectTCP(F(SERVER_ADDRESS), SERVER_PORT);
        connection_attempts++;
    }

    success = wifi.requestURL(packet_buffer);

    if (success) {
        Log.Info(P("Packet uploaded to %s"), SERVER_ADDRESS);
    }else{
        Log.Error(P("Could not upload packet"));
    }

    wifi.closeTCP();
}


void assemble_data_packet(char* packet_buffer){

    char entry[30];

    sprintf(packet_buffer, "%s%s?", GET_REQUEST, DEVICE_NAME);

    for (int i = 0; i < NUM_SOIL_FORKS; i++) {
        sprintf(entry, P("&fork%d=%d"), i, data.soil_fork[i]);
        add_to_array(packet_buffer, entry);
    }

    for (int i = 0; i < NUM_GYPSUM_SENSORS; i++) {
        sprintf(entry, P("&gyps%d=%d"), i, data.gypsum[i]);
        add_to_array(packet_buffer, entry);
    }

    for (int i = 0; i < NUM_LOAD_CELLS; i++) {
        sprintf(entry, P("&mass%d=%d"), i, data.mass[i]);
        add_to_array(packet_buffer, entry);
    }
}


void add_to_array(char* buffer, char* insert){
    int index = strlen(buffer);

    for (int i = 0; i < strlen(insert); i++) {
        buffer[index + i] = insert[i];
    }

    buffer[index + strlen(insert)] = '\0';
}
