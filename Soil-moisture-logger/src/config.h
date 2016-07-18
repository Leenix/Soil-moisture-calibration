#include "Arduino.h"


////////////////////////////////////////////////////////////////////////////////
// Config

const char DEVICE_NAME[] = "soil_calibrator";
#define WIFI_SSID "Handy"
#define WIFI_PASS "things11"
#define SERVER_ADDRESS "www.dweet.io"
const int SERVER_PORT = 80;
const char GET_REQUEST[] = "/dweet/for/";
const char GET_REQUEST_FOOTER[] = "";
const int MAX_CONNECT_ATTEMPTS = 3;
const int WIFI_BAUD = 9600;

const int NUM_SOIL_FORKS = 2;
const int NUM_GYPSUM_SENSORS = 2;
const int NUM_LOAD_CELLS = 2;

const int PACKET_BUFFER_SIZE = 400;
const int DATESTRING_SIZE = 30;

const long SAMPLE_INTERVAL = 30000;
const long UPLOAD_INTERVAL = SAMPLE_INTERVAL * 2;

const float LOAD_CELL_CALIBRATION_FACTOR = 1.0;
const int NUM_LOAD_CELLS_READS = 10;


////////////////////////////////////////////////////////////////////////////////
// Pin assignments

const byte WIFI_TX = 8;
const byte WIFI_RX = 9;

const byte HYGROMETER_1_LEFT = A2;  // Left data pin for hygrometer 1
const byte HYGROMETER_1_RIGHT = A3; // Right data pin for hygrometer 1
const byte HYGROMETER_2_LEFT = A6;  // Left data pin for hygrometer 2
const byte HYGROMETER_2_RIGHT = A7;  // Right data pin for hygrometer 2
const byte HYGROMETER_LEFT[] = {HYGROMETER_1_LEFT, HYGROMETER_2_LEFT};
const byte HYGROMETER_RIGHT[] = {HYGROMETER_1_RIGHT, HYGROMETER_2_RIGHT};
const byte HYGROMETER_LEFT_CONTROL = 6; // Control pin for hygrometer left side
const byte HYGROMETER_RIGHT_CONTROL = 7;    // Control pin for hygrometer right side
const byte HYGROMETER_CONTROL[] = {HYGROMETER_LEFT_CONTROL, HYGROMETER_RIGHT_CONTROL};

const byte SOIL_FORK_1 = A0;    // Data pin for Soil fork 1
const byte SOIL_FORK_2 = A1;    // Data pin for Soil fork 2
const byte SOIL_FORKS[] = {SOIL_FORK_1, SOIL_FORK_2};


////////////////////////////////////////////////////////////////////////////////
// Structs

enum HYGROMETER{
    LEFT = 0,
    RIGHT = 1
};

struct SensorData{
    float mass[NUM_LOAD_CELLS];
	int soil_fork[NUM_SOIL_FORKS];
	int gypsum[NUM_GYPSUM_SENSORS];
};


////////////////////////////////////////////////////////////////////////////////
// Function Prototypes

void start_soil_forks();
int get_soil_fork_moisture(int num_fork);
void update_soil_fork_moisture();

void start_hygrometer();
void hygrometer_off();
int get_gypsum_moisture(int sensor_number);
int calculate_soil_saturation(int gypsum_value);
void update_gypsum_sensors();

void start_wifi();
void connect_wifi();
void upload_packet();
void add_to_array(char* buffer, char* insert);
void send_packet(char* out_message, int message_size);
void check_water_control();
void shut_off_water_control();
void assemble_data_packet(char* packet_buffer);

void start_load_cells();
void update_load_cells();
float get_mass(int load_cell_number);
