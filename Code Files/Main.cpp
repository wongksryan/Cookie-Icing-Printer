#include <cmath>
#include "iq_cpp.h"


using namespace vex;

// Configure sensors
void configureAllSensors()
{
Brain.Screen.print("Calibrating");
BrainInertial.calibrate();
while (BrainInertial.isCalibrating())
{}
Brain.Screen.clearScreen();
Brain.Screen.setCursor(1,1);
BrainInertial.resetRotation();
driveMotors.resetPosition();
Optical3.setLight(ledState::on);
}

