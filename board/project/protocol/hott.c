#include "hott.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airspeed.h"
#include "bmp180.h"
#include "bmp280.h"
#include "common.h"
#include "config.h"
#include "current.h"
#include "esc_apd_f.h"
#include "esc_apd_hv.h"
#include "esc_castle.h"
#include "esc_hw3.h"
#include "esc_hw4.h"
#include "esc_hw5.h"
#include "esc_kontronik.h"
#include "esc_pwm.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "ibus.h"
#include "ms5611.h"
#include "gps.h"
#include "ntc.h"
#include "pico/stdlib.h"
#include "pwm_out.h"
#include "stdlib.h"
#include "string.h"
#include "uart.h"
#include "uart_pio.h"
#include "voltage.h"
#include "smart_esc.h"
#include "esc_omp_m4.h"
#include "esc_ztw.h"

#define HOTT_VARIO_MODULE_ID 0x89
#define HOTT_GPS_MODULE_ID 0x8A
#define HOTT_ESC_MODULE_ID 0x8C
#define HOTT_GENERAL_AIR_MODULE_ID 0x8D  // not used
#define HOTT_ELECTRIC_AIR_MODULE_ID 0x8E

#define HOTT_VARIO_SENSOR_ID 0x90
#define HOTT_GPS_SENSOR_ID 0xA0
#define HOTT_ESC_SENSOR_ID 0xC0
#define HOTT_GENERAL_AIR_SENSOR_ID 0xD0
#define HOTT_ELECTRIC_AIR_SENSOR_ID 0xE0

#define HOTT_TEXT_MODE_REQUEST_GPS 0xAF
#define HOTT_TEXT_MODE_REQUEST_GENERAL_AIR 0xDF
#define HOTT_TEXT_MODE_REQUEST_ELECTRIC_AIR 0xEF

#define HOTT_BINARY_MODE_REQUEST_ID 0x80
#define HOTT_TEXT_MODE_REQUEST_ID 0x7F

#define HOTT_TIMEOUT_US 2000
#define HOTT_PACKET_LENGHT 2

#define HOTT_START_BYTE 0x7C
#define HOTT_END_BYTE 0x7D

// TYPE
#define HOTT_TYPE_VARIO 0
#define HOTT_TYPE_ESC 1
#define HOTT_TYPE_ELECTRIC 2
#define HOTT_TYPE_GPS 3

typedef struct hott_vario_t {
    uint8_t startByte;
    uint8_t sensorID;
    uint8_t warningId;
    uint8_t sensorTextID;
    uint8_t alarmInverse;
    uint16_t altitude;  // value + 500 (e.g. 0m = 500)
    uint16_t maxAltitude;
    uint16_t minAltitude;
    uint16_t m1s;   // ?? (value * 100) + 30000 (e.g. 10m = 31000)
    uint16_t m3s;   // ?? idem
    uint16_t m10s;  // ?? idem
    uint8_t text[24];
    uint8_t empty;
    uint8_t version;
    uint8_t endByte;
    uint8_t checksum;
} __attribute__((packed)) hott_vario_t;

typedef struct hott_airesc_t {
    uint8_t startByte;                  // 1
    uint8_t sensorID;                   // 2
    uint8_t warningId;                  // 3
    uint8_t sensorTextID;               // Byte 4
    uint8_t inverse;                    // Byte 5
    uint8_t inverseStatusI;             // Byte 6
    uint16_t inputVolt;                 // Byte 7,8
    uint16_t minInputVolt;              // Byte 9,10
    uint16_t capacity;                  // Byte 11,12
    uint8_t escTemperature;             // Byte 13
    uint8_t maxEscTemperature;          // Byte 14
    uint16_t current;                   // Byte 15,16
    uint16_t maxCurrent;                // Byte 17,18
    uint16_t RPM;                       // Byte 19,20
    uint16_t maxRPM;                    // Byte 21,22
    uint8_t throttlePercent;            // Byte 23
    uint16_t speed;                     // Byte 24,25
    uint16_t maxSpeed;                  // Byte 26,27
    uint8_t BECVoltage;                 // Byte 28
    uint8_t minBECVoltage;              // Byte 29
    uint8_t BECCurrent;                 // Byte 30
    uint8_t minBECCurrent;              // Byte 31
    uint8_t maxBECCurrent;              // Byte 32
    uint8_t PWM;                        // Byte 33
    uint8_t BECTemperature;             // Byte 34
    uint8_t maxBECTemperature;          // Byte 35
    uint8_t motorOrExtTemperature;      // Byte 36
    uint8_t maxMotorOrExtTemperature;   // Byte 37
    uint16_t RPMWithoutGearOrExt;       // Byte 38,39
    uint8_t timing;                     // Byte 40
    uint8_t advancedTiming;             // Byte 41
    uint8_t highestCurrentMotorNumber;  // Byte 42
    uint8_t version;                    /* Byte 43: 00 version number */
    uint8_t endByte;                    /* Byte 44: 0x7D Ende byte */
    uint8_t checksum;                   /* Byte 45: Parity Byte */
} __attribute__((packed)) hott_airesc_t;

typedef struct hott_electric_air_t {
    uint8_t startByte;      // 1
    uint8_t sensorID;       // 2
    uint8_t alarmTone;      // 3: Alarm
    uint8_t sensorTextID;   // 4:
    uint8_t alarmInverse1;  // 5:
    uint8_t alarmInverse2;  // 6:
    uint8_t cell1L;         // 7: Low Voltage Cell 1 in 0,02 V steps
    uint8_t cell2L;         // 8: Low Voltage Cell 2 in 0,02 V steps
    uint8_t cell3L;         // 9: Low Voltage Cell 3 in 0,02 V steps
    uint8_t cell4L;         // 10: Low Voltage Cell 4 in 0,02 V steps
    uint8_t cell5L;         // 11: Low Voltage Cell 5 in 0,02 V steps
    uint8_t cell6L;         // 12: Low Voltage Cell 6 in 0,02 V steps
    uint8_t cell7L;         // 13: Low Voltage Cell 7 in 0,02 V steps
    uint8_t cell1H;         // 14: High Voltage Cell 1 in 0.02 V steps
    uint8_t cell2H;         // 15
    uint8_t cell3H;         // 16
    uint8_t cell4H;         // 17
    uint8_t cell5H;         // 18
    uint8_t cell6H;         // 19
    uint8_t cell7H;         // 20
    uint16_t battery1;      // 21 Battery 1 in 100mv steps; 50 == 5V
    uint16_t battery2;      // 23 Battery 2 in 100mv steps; 50 == 5V
    uint8_t temp1;          // 25 Temp 1; Offset of 20. 20 == 0C
    uint8_t temp2;          // 26 Temp 2; Offset of 20. 20 == 0C
    uint16_t height;        // 27 28 Height. Offset -500. 500 == 0
    uint16_t current;       // 29 30 1 = 0.1A
    uint16_t driveVoltage;  // 31
    uint16_t capacity;      // 33 34 mAh
    uint16_t m2s;           // 35 36  /* Steigrate m2s; 0x48 == 0
    uint8_t m3s;            // 37  /* Steigrate m3s; 0x78 == 0
    uint16_t rpm;           // 38 39 /* RPM. 10er steps; 300 == 3000rpm
    uint8_t minutes;        // 40
    uint8_t seconds;        // 41
    uint8_t speed;          // 42
    uint8_t version;        // 43
    uint8_t endByte;        // 44
    uint8_t checksum;       // 45
} __attribute__((packed)) hott_electric_air_t;

// general air not used
typedef struct hott_general_air_t {
    uint8_t startByte;             //#01 start byte constant value 0x7c
    uint8_t sensorID;              //#02 EAM sensort id. constat value 0x8d=GENRAL AIR MODULE
    uint8_t alarmTone;             //#03 1=A 2=B ... 0x1a=Z 0 = no alarm
                                   /* VOICE OR BIP WARNINGS
                           Alarme sonore A.. Z, octet correspondant 1 à 26
                           0x00 00 0 No alarm
                           0x01 01 A
                           0x02 02 B Negative Difference 2 B
                           0x03 03 C Negative Difference 1 C
                           0x04 04 D
                           0x05 05 E
                           0x06 06 F Min. Sensor 1 temp. F
                           0x07 07 G Min. Sensor 2 temp. G
                           0x08 08 H Max. Sensor 1 temp. H
                           0x09 09 I Max. Sensor 2 temp. I
                           0xA 10 J Max. Sens. 1 voltage J
                           0xB 11 K Max. Sens. 2 voltage K
                           0xC 12 L
                           0xD 13 M Positive Difference 2 M
                           0xE 14 N Positive Difference 1 N
                           0xF 15 O Min. Altitude O
                           0x10 16 P Min. Power Voltage P // We use this one for Battery Warning
                           0x11 17 Q Min. Cell voltage Q
                           0x12 18 R Min. Sens. 1 voltage R
                           0x13 19 S Min. Sens. 2 voltage S
                           0x14 20 T Minimum RPM T
                           0x15 21 U
                           0x16 22 V Max. used capacity V
                           0x17 23 W Max. Current W
                           0x18 24 X Max. Power Voltage X
                           0x19 25 Y Maximum RPM Y
                           0x1A 26 Z Max. Altitude Z
                           */
    uint8_t sensorTextID;          //#04 constant value 0xd0
    uint8_t alarmInverse1;         //#05 alarm bitmask. Value is displayed inverted
                                   // Bit# Alarm field
                                   // 0 all cell voltage
                                   // 1 Battery 1
                                   // 2 Battery 2
                                   // 3 Temperature 1
                                   // 4 Temperature 2
                                   // 5 Fuel
                                   // 6 mAh
                                   // 7 Altitude
    uint8_t alarm_invers2;         //#06 alarm bitmask. Value is displayed inverted
                                   // Bit# Alarm Field
                                   // 0 main power current
                                   // 1 main power voltage
                                   // 2 Altitude
                                   // 3 m/s
                                   // 4 m/3s
                                   // 5 unknown
                                   // 6 unknown
                                   // 7 "ON" sign/text msg active
    uint8_t cell[6];               //#7 Volt Cell 1 (in 2 mV increments, 210 == 4.20 V)
                                   //#8 Volt Cell 2 (in 2 mV increments, 210 == 4.20 V)
                                   //#9 Volt Cell 3 (in 2 mV increments, 210 == 4.20 V)
                                   //#10 Volt Cell 4 (in 2 mV increments, 210 == 4.20 V)
                                   //#11 Volt Cell 5 (in 2 mV increments, 210 == 4.20 V)
                                   //#12 Volt Cell 6 (in 2 mV increments, 210 == 4.20 V)
    uint16_t battery1;             //#13 LSB battery 1 voltage LSB value. 0.1V steps. 50 = 5.5V only pos. voltages
                                   //#14 MSB
    uint16_t battery2;             //#15 LSB battery 2 voltage LSB value. 0.1V steps. 50 = 5.5V only pos. voltages
                                   //#16 MSB
    uint8_t temperature1;          //#17 Temperature 1. Offset of 20. a value of 20 = 0°C
    uint8_t temperature2;          //#18 Temperature 2. Offset of 20. a value of 20 = 0°C
    uint8_t fuel_procent;          //#19 Fuel capacity in %. Values 0--100
                                   // graphical display ranges: 0-100% with new firmwares of the radios MX12/MX20/...
    uint16_t fuel_ml;              //#20 LSB Fuel in ml scale. Full = 65535!
                                   //#21 MSB
    uint16_t rpm;                  //#22 RPM in 10 RPM steps. 300 = 3000rpm
                                   //#23 MSB
    uint16_t altitude;             //#24 altitude in meters. offset of 500, 500 = 0m
                                   //#25 MSB
    uint16_t climbrate;            //#26 climb rate in 0.01m/s. Value of 30000 = 0.00 m/s
                                   //#27 MSB
    uint8_t climbrate3s;           //#28 climb rate in m/3sec. Value of 120 = 0m/3sec
    uint16_t current;              //#29 current in 0.1A steps 100 == 10,0A
                                   //#30 MSB current display only goes up to 99.9 A (continuous)
    uint16_t main_voltage;         //#31 LSB Main power voltage using 0.1V steps 100 == 10,0V
                                   //#32 MSB (Appears in GAM display right as alternate display.)
    uint16_t batt_cap;             //#33 LSB used battery capacity in 10mAh steps
                                   //#34 MSB
    uint16_t speed;                //#35 LSB (air?) speed in km/h(?) we are using ground speed here per default
                                   //#36 MSB speed
    uint8_t min_cell_volt;         //#37 minimum cell voltage in 2mV steps. 124 = 2,48V
    uint8_t min_cell_volt_num;     //#38 number of the cell with the lowest voltage
    uint16_t rpm2;                 //#39 LSB 2nd RPM in 10 RPM steps. 100 == 1000rpm
                                   //#40 MSB
    uint8_t general_error_number;  //#41 General Error Number (Voice Error == 12) TODO: more documentation
    uint8_t pressure;              //#42 High pressure up to 16bar. 0,1bar scale. 20 == 2.0 bar
                                   // 1 bar = 10 hoch 5 Pa
    uint8_t version;               //#43 version number (Bytes 35 .43 new but not yet in the record in the display!)
    uint8_t endByte;               //#44 stop byte 0x7D
    uint8_t parity;                //#45 CHECKSUM CRC/Parity (calculated dynamicaly)
} __attribute__((packed)) hott_general_air_t;

typedef struct hott_gps_t {
    uint8_t startByte;    /* Byte 1: 0x7C = Start byte data */
    uint8_t sensorID;     /* Byte 2: 0x8A = GPS Sensor */
    uint8_t alarmTone;    /* Byte 3: 0…= warning beeps */
    uint8_t sensorTextID; /* Byte 4: 160 0xA0 Sensor ID Neu! */

    uint8_t alarmInverse1; /* Byte 5: 01 inverse status */
    uint8_t alarmInverse2; /* Byte 6: 00 inverse status status 1 = kein GPS Signal */

    uint8_t
        flightDirection; /* Byte 7: 119 = Flugricht./dir. 1 = 2°; 0° (North), 9 0° (East), 180° (South), 270° (West) */
    uint16_t GPSSpeed; /* Byte 8: 8 = Geschwindigkeit/GPS speed low byte 8km/h */

    uint8_t LatitudeNS;      /* Byte 10: 000 = N = 48°39’988 */
    uint16_t LatitudeDegMin; /* Byte 11: 231 0xE7 = 0x12E7 = 4839 */
    uint16_t LatitudeSec;    /* Byte 13: 171 220 = 0xDC = 0x03DC =0988 */

    uint8_t longitudeEW;      /* Byte 15: 000  = E= 9° 25’9360 */
    uint16_t longitudeDegMin; /* Byte 16: 150 157 = 0x9D = 0x039D = 0925 */
    uint16_t longitudeSec;    /* Byte 18: 056 144 = 0x90 0x2490 = 9360*/

    uint16_t distance;       /* Byte 20: 027 123 = Entfernung/distance low byte 6 = 6 m */
    uint16_t altitude;       /* Byte 22: 243 244 = Höhe/Altitude low byte 500 = 0m */
    uint16_t climbrate;      /* Byte 24: 48 = Low Byte m/s resolution 0.01m 48 = 30000 = 0.00m/s (1=0.01m/s) */
    uint8_t climbrate3s;     /* Byte 26: climbrate in m/3s resolution, value of 120 = 0 m/3s*/
    uint8_t GPSNumSat;       /* Byte 27: GPS.Satelites (number of satelites) (1 byte) */
    uint8_t GPSFixChar;      /* Byte 28: GPS.FixChar. (GPS fix character. display, if DGPS, 2D oder 3D) (1 byte) */
    uint8_t homeDirection;   /* Byte 29: HomeDirection (direction from starting point to Model position) (1 byte) */
    uint8_t angleXdirection; /* Byte 30: angle x-direction (1 byte) */
    uint8_t angleYdirection; /* Byte 31: angle y-direction (1 byte) */
    uint8_t angleZdirection; /* Byte 32: angle z-direction (1 byte) */

    uint8_t gps_time_h;  //#33 UTC time hours
    uint8_t gps_time_m;  //#34 UTC time minutes
    uint8_t gps_time_s;  //#35 UTC time seconds
    uint8_t gps_time_sss;//#36 UTC time milliseconds
    uint16_t msl_altitude;  //#37 mean sea level altitude

    uint8_t vibration; /* Byte 39: vibration (1 bytes) */
    uint8_t Ascii4;    /* Byte 40: 00 ASCII Free Character [4] appears right to home distance */
    uint8_t Ascii5;    /* Byte 41: 00 ASCII Free Character [5] appears right to home direction*/
    uint8_t GPS_fix;   /* Byte 42: 00 ASCII Free Character [6], we use it for GPS FIX */
    uint8_t version;   /* Byte 43: 00 version number */
    uint8_t endByte;   /* Byte 44: 0x7D Ende byte */
    uint8_t checksum;  /* Byte 45: Parity Byte */
} __attribute__((packed)) hott_gps_t;

typedef struct hott_sensor_vario_t {
    float *altitude;
    float *m1s;
    float *m3s;
    float *m10s;
} hott_sensor_vario_t;

typedef struct hott_sensor_airesc_t {
    float *voltage;
    float *consumption;
    float *temperature;
    float *current;
    float *rpm;
    float *speed;
    float *voltage_bec;
    float *current_bec;
    float *temperature_bec;
    float *temperature_ext;
} hott_sensor_airesc_t;

typedef struct hott_sensor_electric_air_t {
    float *temperature_ext;
    float *bat1_cell;
    float *bat2_cell;
    float *bat1_volt;
    float *bat2_volt;
    float *bat1_temp;
    float *bat2_temp;
    float *height;
    float *current;
    float *consumption;
    float *rpm;
    float *speed;
} hott_sensor_electric_air_t;

typedef struct hott_sensor_gps_t {
    float *direction;
    float *speed;
    double *latitude;
    double *longitude;
    float *distance;
    float *altitude;
    float *climbrate;
    float *climbrate3s;
    float *sats;
} hott_sensor_gps_t;

typedef struct hott_sensors_t {
    bool is_enabled[4];
    hott_sensor_vario_t vario;
    hott_sensor_airesc_t esc;
    hott_sensor_electric_air_t electric_air;
    hott_sensor_gps_t gps;
} hott_sensors_t;

typedef struct vario_alarm_parameters_t {
    float *altitude;
    uint16_t m1s;
    uint16_t m3s;
    uint16_t m10s;
} vario_alarm_parameters_t;

typedef struct hott_parameters_t {
    vario_alarm_parameters_t vario_alarm_parameters;
    float *baro_temp, *baro_pressure;
} hott_parameters_t;

vario_alarm_parameters_t *vario_alarm_parameters;

static void process(hott_sensors_t *sensors);
static void send_packet(hott_sensors_t *sensors, uint8_t address);
static uint8_t get_crc(const uint8_t *buffer, uint len);
static void set_config(hott_sensors_t *sensors);
static int64_t interval_1000_callback(alarm_id_t id, void *parameters);
static int64_t interval_3000_callback(alarm_id_t id, void *parameters);
static int64_t interval_10000_callback(alarm_id_t id, void *parameters);

void hott_task(void *parameters) {
    hott_sensors_t sensors = {0};
    set_config(&sensors);
    context.led_cycle_duration = 6;
    context.led_cycles = 1;
    uart0_begin(19200, UART_RECEIVER_TX, UART_RECEIVER_RX, HOTT_TIMEOUT_US, 8, 1, UART_PARITY_NONE, false, true);
    debug("\nHOTT init");
    while (1) {
        ulTaskNotifyTakeIndexed(1, pdTRUE, portMAX_DELAY);
        process(&sensors);
    }
}

static void process(hott_sensors_t *sensors) {
    uint8_t len = uart0_available();
    if (len == HOTT_PACKET_LENGHT || len == HOTT_PACKET_LENGHT + 1) {
        uint8_t buffer[len];
        uart0_read_bytes(buffer, len);
        debug("\nHOTT (%u) < ", uxTaskGetStackHighWaterMark(NULL));
        debug_buffer(buffer, len, "0x%X ");
        if (buffer[0] == HOTT_BINARY_MODE_REQUEST_ID) send_packet(sensors, buffer[1]);
    }
}

static void send_packet(hott_sensors_t *sensors, uint8_t address) {
    // packet in little endian
    switch (address) {
        case HOTT_VARIO_MODULE_ID: {
            static uint16_t max_altitude = 0, min_altitude = 0;
            if (!sensors->is_enabled[HOTT_TYPE_VARIO]) return;
            hott_vario_t packet = {0};
            packet.startByte = HOTT_START_BYTE;
            packet.sensorID = HOTT_VARIO_MODULE_ID;
            packet.sensorTextID = HOTT_VARIO_SENSOR_ID;
            packet.altitude = *sensors->vario.altitude + 500;
            if (max_altitude < packet.altitude) max_altitude = packet.altitude;
            if (min_altitude > packet.altitude) min_altitude = packet.altitude;
            packet.maxAltitude = max_altitude;
            packet.minAltitude = min_altitude;
            packet.m1s = vario_alarm_parameters->m1s;
            packet.m3s = vario_alarm_parameters->m3s;
            packet.m10s = vario_alarm_parameters->m10s;
            packet.endByte = HOTT_END_BYTE;
            packet.checksum = get_crc((uint8_t *)&packet, sizeof(packet) - 1);
            uart0_write_bytes((uint8_t *)&packet, sizeof(packet));
            debug("\nHOTT (%u) %u > ", uxTaskGetStackHighWaterMark(NULL), sizeof(packet));
            debug_buffer((uint8_t *)&packet, sizeof(packet), "0x%X ");

            // blink led
            vTaskResume(context.led_task_handle);
            break;
        }
        case HOTT_ESC_MODULE_ID: {
            if (!sensors->is_enabled[HOTT_TYPE_ESC]) return;
            hott_airesc_t packet = {0};
            static uint16_t minInputVolt = 0xFFFF;
            static uint8_t maxEscTemperature = 0;
            static uint16_t maxCurrent = 0;
            static uint16_t maxRPM = 0;
            static uint16_t maxSpeed = 0;
            static uint16_t minBECCurrent = 0xFFFF;
            static uint16_t maxBECCurrent = 0;
            static uint16_t minBECVoltage = 0xFFFF;
            static uint16_t maxBECTemperature = 0;
            static uint16_t maxMotorOrExtTemperature = 0;
            packet.startByte = HOTT_START_BYTE;
            packet.sensorID = HOTT_ESC_MODULE_ID;
            packet.sensorTextID = HOTT_ESC_SENSOR_ID;
            if (sensors->esc.voltage_bec) {
                packet.inputVolt = *sensors->esc.voltage_bec * 10;
                if (packet.inputVolt > minInputVolt) packet.minInputVolt = packet.inputVolt;
            }
            if (sensors->esc.temperature) {
                packet.escTemperature = *sensors->esc.temperature + 20;
                if (packet.escTemperature > maxEscTemperature) packet.maxEscTemperature = packet.escTemperature;
            }
            if (sensors->esc.current_bec) {
                packet.current = *sensors->esc.current_bec * 10;
                if (packet.current > maxCurrent) packet.maxCurrent = packet.current;
                packet.capacity = *sensors->esc.consumption / 10;
            }
            if (sensors->esc.rpm) {
                packet.RPM = *sensors->esc.rpm / 10;
                if (packet.RPM > maxRPM) packet.maxRPM = packet.RPM;
            }
            // uint8_t throttlePercent;            // Byte 22
            if (sensors->esc.speed) {
                packet.speed = *sensors->esc.speed;
                if (packet.speed > maxSpeed) packet.maxSpeed = packet.speed;
            }
            if (sensors->esc.voltage_bec) {
                packet.BECVoltage = *sensors->esc.voltage_bec * 10;
                if (packet.minBECVoltage < minBECVoltage) packet.minBECVoltage = packet.BECVoltage;
            }
            if (sensors->esc.current_bec) {
                packet.BECCurrent = *sensors->esc.current_bec * 10;
                if (packet.BECCurrent < minBECCurrent) packet.minBECCurrent = packet.BECCurrent;
                if (packet.BECCurrent > maxBECCurrent) packet.maxBECCurrent = packet.BECCurrent;
            }
            // uint8_t PWM;                        // Byte 32
            if (sensors->esc.temperature_bec) {
                packet.BECTemperature = *sensors->esc.temperature_bec + 20;
                if (packet.BECTemperature > maxBECTemperature) packet.maxBECTemperature = packet.BECTemperature;
            }
            if (sensors->esc.temperature_ext) {
                packet.motorOrExtTemperature = *sensors->esc.temperature_ext + 20;
                if (packet.motorOrExtTemperature > maxMotorOrExtTemperature)
                    packet.maxMotorOrExtTemperature = packet.motorOrExtTemperature;
            }
            // uint16_t RPMWithoutGearOrExt;       // Byte 37
            // uint8_t timing;                     // Byte 39
            // uint8_t advancedTiming;             // Byte 40
            // uint8_t highestCurrentMotorNumber;  // Byte 41
            packet.endByte = HOTT_END_BYTE;
            packet.checksum = get_crc((uint8_t *)&packet, sizeof(packet) - 1);
            uart0_write_bytes((uint8_t *)&packet, sizeof(packet));
            debug("\nHOTT (%u) Len: %u > ", uxTaskGetStackHighWaterMark(NULL), sizeof(packet));
            debug_buffer((uint8_t *)&packet, sizeof(packet), "0x%X ");

            // blink led
            vTaskResume(context.led_task_handle);
            break;
        }
        case HOTT_ELECTRIC_AIR_MODULE_ID: {
            if (!sensors->is_enabled[HOTT_TYPE_ELECTRIC]) return;
            hott_electric_air_t packet = {0};
            packet.startByte = HOTT_START_BYTE;
            packet.sensorID = HOTT_ELECTRIC_AIR_MODULE_ID;
            packet.sensorTextID = HOTT_ELECTRIC_AIR_SENSOR_ID;
            if (sensors->electric_air.bat1_volt)
                packet.battery1 = *sensors->electric_air.bat1_volt * 10;
            if (sensors->electric_air.current)
                packet.current = *sensors->electric_air.current * 10;
            if (sensors->electric_air.bat1_temp)
                packet.temp1 = *sensors->electric_air.bat1_temp + 20;
            packet.endByte = HOTT_END_BYTE;
            packet.checksum = get_crc((uint8_t *)&packet, sizeof(packet) - 1);
            uart0_write_bytes((uint8_t *)&packet, sizeof(packet));
            debug("\nHOTT (%u) %u > ", uxTaskGetStackHighWaterMark(NULL), sizeof(packet));
            debug_buffer((uint8_t *)&packet, sizeof(packet), "0x%X ");

            // blink led
            vTaskResume(context.led_task_handle);
            break;
        }
        case HOTT_GPS_MODULE_ID: {
            if (!sensors->is_enabled[HOTT_TYPE_GPS]) return;
            hott_gps_t packet = {0};
            packet.startByte = HOTT_START_BYTE;
            packet.sensorID = HOTT_GPS_MODULE_ID;
            packet.sensorTextID = HOTT_GPS_SENSOR_ID;
            packet.flightDirection = *sensors->gps.direction / 2;  // 0.5°
            packet.GPSSpeed = *sensors->gps.speed;                 // km/h
            packet.LatitudeNS = sensors->gps.latitude > 0 ? 0 : 1;
            double latitude = fabs(*sensors->gps.latitude);
            packet.LatitudeDegMin = (uint)latitude * 100 + (latitude - (uint)latitude) * 60;
            packet.LatitudeSec = (latitude * 60 - (uint)(latitude * 60)) * 60;
            packet.longitudeEW = sensors->gps.longitude > 0 ? 0 : 1;
            double longitude = fabs(*sensors->gps.longitude);
            packet.longitudeDegMin = (uint)latitude * 100 + (latitude - (uint)latitude) * 60;
            packet.longitudeSec = (longitude * 60 - (uint)(longitude * 60)) * 60;
            //packet.distance = *sensors->gps.distance;
            packet.altitude = *sensors->gps.altitude + 500;
            // packet.climbrate = *sensors->gps.altitude;    // 30000, 0.00
            // packet.climbrate3s = *sensors->gps.altitude;  // 120, 0
            packet.GPSNumSat = *sensors->gps.sats;
            // uint8_t GPSFixChar;      // Byte 28: GPS.FixChar. (GPS fix character. display, if DGPS, 2D oder 3D) (1 byte)
            // uint8_t homeDirection;   // Byte 29: HomeDirection (direction from starting point to Model position) (1 byte)
            // uint8_t gps_time_h;  //#33 UTC time hours int8_t gps_time_m;  //#34 UTC time minutes
            // uint8_t gps_time_s;  //#35 UTC time seconds
            // uint8_t gps_time_sss;//#36 UTC time milliseconds
            // uint8_t msl_altitude;//#37 mean sea level altitudeuint8_t vibration;
            // uint8_t vibration; // Byte 39 vibrations level in %
            // uint8_t Ascii4;    // Byte 40: 00 ASCII Free Character [4] appears right to home distance
            // uint8_t Ascii5;    // Byte 41: 00 ASCII Free Character [5] appears right to home direction
            // uint8_t GPS_fix;   // Byte 42: 00 ASCII Free Character [6], we use it for GPS FIX
            // uint8_t version;   // Byte 43: 00 version number*/

            packet.endByte = HOTT_END_BYTE;
            packet.checksum = get_crc((uint8_t *)&packet, sizeof(packet) - 1);
            uart0_write_bytes((uint8_t *)&packet, sizeof(packet));
            debug("\nHOTT (%u) %u > ", uxTaskGetStackHighWaterMark(NULL), sizeof(packet));
            debug_buffer((uint8_t *)&packet, sizeof(packet), "0x%X ");

            // blink led
            vTaskResume(context.led_task_handle);
            break;
        }
    }
}

static uint8_t get_crc(const uint8_t *buffer, uint len) {
    uint16_t crc = 0;
    for (uint i = 0; i < len; i++) {
        crc += buffer[i];
    }
    return crc;
}

static void set_config(hott_sensors_t *sensors) {
    config_t *config = config_read();
    TaskHandle_t task_handle;
    float *baro_temp = NULL, *baro_pressure = NULL;
    vario_alarm_parameters = calloc(1, sizeof(vario_alarm_parameters_t));
    if (config->esc_protocol == ESC_PWM) {
        esc_pwm_parameters_t parameter = {config->rpm_multiplier, config->alpha_rpm, malloc(sizeof(float))};
        xTaskCreate(esc_pwm_task, "esc_pwm_task", STACK_ESC_PWM, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
    }
    if (config->esc_protocol == ESC_HW3) {
        esc_hw3_parameters_t parameter = {config->rpm_multiplier, config->alpha_rpm, malloc(sizeof(float))};
        xTaskCreate(esc_hw3_task, "esc_hw3_task", STACK_ESC_HW3, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
    }
    if (config->esc_protocol == ESC_HW4) {
        esc_hw4_parameters_t parameter = {config->rpm_multiplier,
                                          config->enable_pwm_out,
                                          config->enable_esc_hw4_init_delay,
                                          config->alpha_rpm,
                                          config->alpha_voltage,
                                          config->alpha_current,
                                          config->alpha_temperature,
                                          config->esc_hw4_divisor,
                                          config->esc_hw4_current_multiplier,
                                          config->esc_hw4_current_thresold,
                                          config->esc_hw4_current_max,
                                          config->esc_hw4_is_manual_offset,
                                          config->esc_hw4_offset,
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(uint8_t))};
        xTaskCreate(esc_hw4_task, "esc_hw4_task", STACK_ESC_HW4, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (config->enable_pwm_out) {
            xTaskCreate(pwm_out_task, "pwm_out", STACK_PWM_OUT, (void *)parameter.rpm, 2, &task_handle);
            context.pwm_out_task_handle = task_handle;
            xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature_fet;
        sensors->esc.temperature_bec = parameter.temperature_bec;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.consumption = parameter.consumption;
    }
    if (config->esc_protocol == ESC_HW5) {
        esc_hw5_parameters_t parameter = {
            config->rpm_multiplier,    config->alpha_rpm,     config->alpha_voltage, config->alpha_current,
            config->alpha_temperature, malloc(sizeof(float)), malloc(sizeof(float)), malloc(sizeof(float)),
            malloc(sizeof(float)),     malloc(sizeof(float)), malloc(sizeof(float)), malloc(sizeof(float)),
            malloc(sizeof(float)),     malloc(sizeof(float)), malloc(sizeof(float)), malloc(sizeof(uint8_t))};
        xTaskCreate(esc_hw5_task, "esc_hw5_task", STACK_ESC_HW5, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature_fet;
        sensors->esc.temperature_bec = parameter.temperature_bec;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.voltage_bec = parameter.voltage_bec;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.current_bec = parameter.current_bec;
        sensors->esc.consumption = parameter.consumption;
        sensors->esc.temperature_ext = parameter.temperature_motor;
    }
    if (config->esc_protocol == ESC_CASTLE) {
        esc_castle_parameters_t parameter = {config->rpm_multiplier, config->alpha_rpm,         config->alpha_voltage,
                                             config->alpha_current,  config->alpha_temperature, malloc(sizeof(float)),
                                             malloc(sizeof(float)),  malloc(sizeof(float)),     malloc(sizeof(float)),
                                             malloc(sizeof(float)),  malloc(sizeof(float)),     malloc(sizeof(float)),
                                             malloc(sizeof(float)),  malloc(sizeof(float)),     malloc(sizeof(float)),
                                             malloc(sizeof(float)),  malloc(sizeof(uint8_t))};
        xTaskCreate(esc_castle_task, "esc_castle_task", STACK_ESC_CASTLE, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.voltage_bec = parameter.voltage_bec;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.current_bec = parameter.current_bec;
        sensors->esc.consumption = parameter.consumption;
        sensors->esc.temperature_ext = parameter.consumption;
    }
    if (config->esc_protocol == ESC_KONTRONIK) {
        esc_kontronik_parameters_t parameter = {
            config->rpm_multiplier,    config->alpha_rpm,     config->alpha_voltage,  config->alpha_current,
            config->alpha_temperature, malloc(sizeof(float)), malloc(sizeof(float)),  malloc(sizeof(float)),
            malloc(sizeof(float)),     malloc(sizeof(float)), malloc(sizeof(float)),  malloc(sizeof(float)),
            malloc(sizeof(float)),     malloc(sizeof(float)), malloc(sizeof(uint8_t))};
        xTaskCreate(esc_kontronik_task, "esc_kontronik_task", STACK_ESC_KONTRONIK, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature_fet;
        sensors->esc.temperature_bec = parameter.temperature_bec;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.voltage_bec = parameter.voltage_bec;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.current_bec = parameter.current_bec;
        sensors->esc.consumption = parameter.consumption;
    }
    if (config->esc_protocol == ESC_APD_F) {
        esc_apd_f_parameters_t parameter = {config->rpm_multiplier, config->alpha_rpm,         config->alpha_voltage,
                                            config->alpha_current,  config->alpha_temperature, malloc(sizeof(float)),
                                            malloc(sizeof(float)),  malloc(sizeof(float)),     malloc(sizeof(float)),
                                            malloc(sizeof(float)),  malloc(sizeof(float)),     malloc(sizeof(uint8_t))};
        xTaskCreate(esc_apd_f_task, "esc_apd_f_task", STACK_ESC_APD_F, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.consumption = parameter.consumption;
    }
    if (config->esc_protocol == ESC_APD_HV) {
        esc_apd_hv_parameters_t parameter = {
            config->rpm_multiplier,    config->alpha_rpm,     config->alpha_voltage, config->alpha_current,
            config->alpha_temperature, malloc(sizeof(float)), malloc(sizeof(float)), malloc(sizeof(float)),
            malloc(sizeof(float)),     malloc(sizeof(float)), malloc(sizeof(float)), malloc(sizeof(uint8_t))};
        xTaskCreate(esc_apd_hv_task, "esc_apd_hv_task", STACK_ESC_APD_HV, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.consumption = parameter.consumption;
    }
    if (config->esc_protocol == ESC_SMART) {
        smart_esc_parameters_t parameter;
        parameter.rpm_multiplier = config->rpm_multiplier;
        parameter.alpha_rpm = config->alpha_rpm;
        parameter.alpha_voltage = config->alpha_voltage;
        parameter.alpha_current = config->alpha_current;
        parameter.alpha_temperature = config->alpha_temperature;
        parameter.rpm = malloc(sizeof(float));
        parameter.voltage = malloc(sizeof(float));
        parameter.current = malloc(sizeof(float));
        parameter.temperature_fet = malloc(sizeof(float));
        parameter.temperature_bec = malloc(sizeof(float));
        parameter.voltage_bec = malloc(sizeof(float));
        parameter.current_bec = malloc(sizeof(float));
        parameter.temperature_bat = malloc(sizeof(float));
        parameter.current_bat = malloc(sizeof(float));
        parameter.consumption = malloc(sizeof(float));
        for (uint i = 0; i < 18; i++) parameter.cell[i] = malloc(sizeof(float));
        parameter.cells = malloc(sizeof(uint8_t));
        parameter.cycles = malloc(sizeof(uint16_t));
        xTaskCreate(smart_esc_task, "smart_esc_task", STACK_SMART_ESC, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temperature_fet;
        sensors->esc.temperature_bec = parameter.temperature_bec;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.voltage_bec = parameter.voltage_bec;
        sensors->esc.current_bec = parameter.current_bec;

        sensors->is_enabled[HOTT_TYPE_ELECTRIC] = true;
        sensors->electric_air.bat1_temp = parameter.temperature_bat;
        sensors->electric_air.current = parameter.current_bat;
        sensors->electric_air.consumption = parameter.consumption;
    }
    if (config->esc_protocol == ESC_OMP_M4) {
        esc_omp_m4_parameters_t parameter;
        parameter.rpm_multiplier = config->rpm_multiplier;
        parameter.alpha_rpm = config->alpha_rpm;
        parameter.alpha_voltage = config->alpha_voltage;
        parameter.alpha_current = config->alpha_current;
        parameter.alpha_temperature = config->alpha_temperature;
        parameter.rpm = malloc(sizeof(float));
        parameter.voltage = malloc(sizeof(float));
        parameter.current = malloc(sizeof(float));
        parameter.temp_esc = malloc(sizeof(float));
        parameter.temp_motor = malloc(sizeof(float));
        parameter.cell_voltage = malloc(sizeof(float));
        parameter.consumption = malloc(sizeof(float));
        parameter.cell_count = malloc(sizeof(uint8_t));
        xTaskCreate(esc_omp_m4_task, "esc_omp_m4_task", STACK_SMART_ESC, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temp_esc;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.consumption = parameter.consumption;
        sensors->esc.temperature_ext = parameter.temp_motor;

    }
    if (config->esc_protocol == ESC_ZTW) {
        esc_ztw_parameters_t parameter;
        parameter.rpm_multiplier = config->rpm_multiplier;
        parameter.alpha_rpm = config->alpha_rpm;
        parameter.alpha_voltage = config->alpha_voltage;
        parameter.alpha_current = config->alpha_current;
        parameter.alpha_temperature = config->alpha_temperature;
        parameter.rpm = malloc(sizeof(float));
        parameter.voltage = malloc(sizeof(float));
        parameter.current = malloc(sizeof(float));
        parameter.temp_esc = malloc(sizeof(float));
        parameter.temp_motor = malloc(sizeof(float));
        parameter.bec_voltage = malloc(sizeof(float));
        parameter.cell_voltage = malloc(sizeof(float));
        parameter.consumption = malloc(sizeof(float));
        parameter.cell_count = malloc(sizeof(uint8_t));
        xTaskCreate(esc_omp_m4_task, "esc_ztw_task", STACK_ESC_ZTW, (void *)&parameter, 2, &task_handle);
        context.uart1_notify_task_handle = task_handle;
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.rpm = parameter.rpm;
        sensors->esc.temperature = parameter.temp_esc;
        sensors->esc.voltage_bec = parameter.voltage;
        sensors->esc.current_bec = parameter.current;
        sensors->esc.consumption = parameter.consumption;
        sensors->esc.temperature_ext = parameter.temp_motor;
    }
    if (config->enable_gps) {
        gps_parameters_t parameter;
        parameter.protocol = config->gps_protocol;
        parameter.baudrate = config->gps_baudrate;
        parameter.rate = config->gps_rate;
        parameter.lat = malloc(sizeof(double));
        parameter.lon = malloc(sizeof(double));
        parameter.alt = malloc(sizeof(float));
        parameter.spd = malloc(sizeof(float));
        parameter.cog = malloc(sizeof(float));
        parameter.hdop = malloc(sizeof(float));
        parameter.sat = malloc(sizeof(float));
        parameter.time = malloc(sizeof(float));
        parameter.date = malloc(sizeof(float));
        parameter.vspeed = malloc(sizeof(float));
        parameter.dist = malloc(sizeof(float));
        parameter.spd_kmh = malloc(sizeof(float));
        parameter.fix = malloc(sizeof(float));
        parameter.vdop = malloc(sizeof(float));
        parameter.speed_acc = malloc(sizeof(float));
        parameter.h_acc = malloc(sizeof(float));
        parameter.v_acc = malloc(sizeof(float));
        parameter.track_acc = malloc(sizeof(float));
        parameter.n_vel = malloc(sizeof(float));
        parameter.e_vel = malloc(sizeof(float));
        parameter.v_vel = malloc(sizeof(float));
        parameter.alt_elipsiod = malloc(sizeof(float));
        parameter.dist = malloc(sizeof(float));
        xTaskCreate(gps_task, "gps_task", STACK_GPS, (void *)&parameter, 2, &task_handle);
        context.uart_pio_notify_task_handle = task_handle;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_GPS] = true;
        sensors->gps.latitude = parameter.lat;
        sensors->gps.longitude = parameter.lon;
        sensors->gps.sats = parameter.sat;
        sensors->gps.altitude = parameter.alt;
        sensors->gps.speed = parameter.spd_kmh;
        sensors->gps.direction = parameter.cog;
    }
    if (config->enable_analog_voltage) {
        voltage_parameters_t parameter = {0, config->analog_rate, config->alpha_voltage,
                                          config->analog_voltage_multiplier, malloc(sizeof(float))};
        xTaskCreate(voltage_task, "voltage_task", STACK_VOLTAGE, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ELECTRIC] = true;
        sensors->electric_air.bat1_volt = parameter.voltage;
    }
    if (config->enable_analog_current) {
        current_parameters_t parameter = {1,
                                          config->analog_rate,
                                          config->alpha_current,
                                          config->analog_current_multiplier,
                                          config->analog_current_offset,
                                          config->analog_current_autoffset,
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float)),
                                          malloc(sizeof(float))};
        xTaskCreate(current_task, "current_task", STACK_CURRENT, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ELECTRIC] = true;
        sensors->electric_air.current = parameter.current;
    }
    if (config->enable_analog_ntc) {
        ntc_parameters_t parameter = {2, config->analog_rate, config->alpha_temperature, malloc(sizeof(float))};
        xTaskCreate(ntc_task, "ntc_task", STACK_NTC, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ELECTRIC] = true;
        sensors->electric_air.bat1_temp = parameter.ntc;
    }
    if (config->enable_analog_airspeed) {
        airspeed_parameters_t parameter = {3,
                                           config->analog_rate,
                                           config->alpha_airspeed,
                                           (float)config->airspeed_offset / 100,
                                           (float)config->airspeed_slope / 100,
                                           baro_temp,
                                           baro_pressure,
                                           malloc(sizeof(float))};
        xTaskCreate(airspeed_task, "airspeed_task", STACK_AIRSPEED, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        sensors->is_enabled[HOTT_TYPE_ESC] = true;
        sensors->esc.speed = parameter.airspeed;
    }
    if (config->i2c_module == I2C_BMP280) {
        bmp280_parameters_t parameter = {config->alpha_vario,   config->vario_auto_offset, config->i2c_address,
                                         config->bmp280_filter, malloc(sizeof(float)),     malloc(sizeof(float)),
                                         malloc(sizeof(float)), malloc(sizeof(float))};
        xTaskCreate(bmp280_task, "bmp280_task", STACK_BMP280, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (config->enable_analog_airspeed) {
            baro_temp = parameter.temperature;
            baro_pressure = parameter.pressure;
        }

        sensors->is_enabled[HOTT_TYPE_VARIO] = true;
        sensors->vario.altitude = parameter.altitude;

        add_alarm_in_ms(1000, interval_1000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(3000, interval_3000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(10000, interval_10000_callback, vario_alarm_parameters, false);
    }
    if (config->i2c_module == I2C_MS5611) {
        ms5611_parameters_t parameter = {config->alpha_vario,   config->vario_auto_offset, config->i2c_address,
                                         malloc(sizeof(float)), malloc(sizeof(float)),     malloc(sizeof(float)),
                                         malloc(sizeof(float))};
        xTaskCreate(ms5611_task, "ms5611_task", STACK_MS5611, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (config->enable_analog_airspeed) {
            baro_temp = parameter.temperature;
            baro_pressure = parameter.pressure;
        }

        sensors->is_enabled[HOTT_TYPE_VARIO] = true;
        sensors->vario.altitude = parameter.altitude;

        add_alarm_in_ms(1000, interval_1000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(3000, interval_3000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(10000, interval_10000_callback, vario_alarm_parameters, false);
    }
    if (config->i2c_module == I2C_BMP180) {
        bmp180_parameters_t parameter = {config->alpha_vario,   config->vario_auto_offset, config->i2c_address,
                                         malloc(sizeof(float)), malloc(sizeof(float)),     malloc(sizeof(float)),
                                         malloc(sizeof(float))};
        xTaskCreate(bmp180_task, "bmp180_task", STACK_BMP180, (void *)&parameter, 2, &task_handle);
        xQueueSendToBack(context.tasks_queue_handle, task_handle, 0);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (config->enable_analog_airspeed) {
            baro_temp = parameter.temperature;
            baro_pressure = parameter.pressure;
        }

        sensors->is_enabled[HOTT_TYPE_VARIO] = true;
        sensors->vario.altitude = parameter.altitude;

        add_alarm_in_ms(1000, interval_1000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(3000, interval_3000_callback, vario_alarm_parameters, false);
        add_alarm_in_ms(10000, interval_10000_callback, vario_alarm_parameters, false);
    }
}

static int64_t interval_1000_callback(alarm_id_t id, void *parameters) {
    vario_alarm_parameters_t *parameter = (vario_alarm_parameters_t *)parameters;
    static float prev = 0;
    parameter->m1s = (*parameter->altitude - prev) * 100 + 30000;
#ifdef SIM_SENSORS
    //vario_alarm_parameters.m1s = 12 * 100 + 30000;
#endif
    prev = *parameter->altitude;
    return 1000000L;
}

static int64_t interval_3000_callback(alarm_id_t id, void *parameters) {
    vario_alarm_parameters_t *parameter = (vario_alarm_parameters_t *)parameters;
    static float prev = 0;
    parameter->m3s = (*parameter->altitude - prev) * 100 + 30000;
#ifdef SIM_SENSORS
    //vario_alarm_parameters.m3s = 34 * 100 + 30000;
#endif
    prev = *parameter->altitude;
    return 3000000L;
}

static int64_t interval_10000_callback(alarm_id_t id, void *parameters) {
    vario_alarm_parameters_t *parameter = (vario_alarm_parameters_t *)parameters;
    static float prev = 0;
    parameter->m10s = (*parameter->altitude - prev) * 100 + 30000;
#ifdef SIM_SENSORS
    //vario_alarm_parameters.m10s = 56 * 100 + 30000;
#endif
    prev = *parameter->altitude;
    return 10000000L;
}
