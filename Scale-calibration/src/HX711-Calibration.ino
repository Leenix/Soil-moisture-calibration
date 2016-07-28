#include "HX711.h"
#include "logging.h"
#include "ProgmemString.h"
#include <SimpleTimer.h>

////////////////////////////////////////////////////////////////////////////////
// Objects

HX711 scale(2, 3);

SimpleTimer timer;
float target = 0;

////////////////////////////////////////////////////////////////////////////////
// Main Functions

void setup(){
    /**
    * Initialise the calibration test.
    */
    Log.Init(LOG_LEVEL_VERBOSE, 57600);
    Log.Info(P("Soil moisture calibration logger"));

    Log.Info(P("Resetting scales"));
    scale.set_scale();
    scale.tare();

    Log.Info("Ready to calibrate:\na:\tTarget mass +100g\ns:\tTarget mass +10g\nd:\tTarget mass +1g\nz:\tTarget mass -100g\nx:\tTarget mass -10g\nc:\tTarget mass -1g\nEnter:\tStart calibration");
}

void loop(){

    while (!Serial.available()){
        delay(10);
    }

    char command = Serial.read();
    switch(command){
        case 'z':
            add_to_target(-100);
            break;

        case 'x':
            add_to_target(-10);
            break;

        case 'c':
            add_to_target(-1);
            break;

        case 'a':
            add_to_target(100);
            break;

        case 's':
            add_to_target(10);
            break;

        case 'd':
            add_to_target(1);
            break;

        case '\n':
            run_test(target);
            break;

        case 'q':
            read_scales();
            break;

        case 't':
            scale.tare(5);
            break;
    }
}

void add_to_target(int increment){
    target += increment;
    target = constrain(target, 0, 5000);

    Log.Info("New target: %d", int(target));

}

void run_test(float target){
    Log.Info("Calibrating to target: %dg", int(target));
    float scaling_factor = 1;
    scale.set_scale(scaling_factor);
    float actual = unsigned(scale.get_units(10));

    float increment = 500;
    bool last_polarity = true;
    while (int(target) != int(actual) && increment > 0.0001){
        bool polarity;

        if (actual < target){
            scaling_factor -= increment;
            polarity = false;
        }else{
            scaling_factor += increment;
            polarity = true;
        }

        scaling_factor = constrain(scaling_factor, 0, 10000);

        scale.set_scale(scaling_factor);
        actual = unsigned(scale.get_units(10));
        Log.Debug("Scaling factor: %d\nNew reading: %dg", int(scaling_factor), int(actual));

        if (polarity != last_polarity){
            increment *= 0.5;
        }
        last_polarity = polarity;
    }

    char value[10];
    dtostrf(scaling_factor, 0, 4, value);
    Log.Info("Scaling factor: %s", value);
}

void read_scales(){
    float mass = scale.get_units(10);
    char value[8];
    dtostrf(mass, 0, 2, value);
    Log.Info("Mass: %sg", value);
}
