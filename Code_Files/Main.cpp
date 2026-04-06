// ============================================================
//  VEXcode IQ Generated Robot Configuration
// ============================================================
#pragma region VEXcode Generated Robot Configuration
// Make sure all required headers are included.
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>


#include "vex.h"

using namespace vex;

// Brain should be defined by default
brain Brain;


// START IQ MACROS
#define waitUntil(condition)                                                   \
  do {                                                                         \
    wait(5, msec);                                                             \
  } while (!(condition))

#define repeat(iterations)                                                     \
  for (int iterator = 0; iterator < iterations; iterator++)
// END IQ MACROS


// Robot configuration code.
inertial BrainInertial = inertial();


// generating and setting random seed
void initializeRandomSeed(){
  wait(100,msec);
  double xAxis = BrainInertial.acceleration(xaxis) * 1000;
  double yAxis = BrainInertial.acceleration(yaxis) * 1000;
  double zAxis = BrainInertial.acceleration(zaxis) * 1000;
  // Combine these values into a single integer
  int seed = int(
    xAxis + yAxis + zAxis
  );
  // Set the seed
  srand(seed); 
}



void vexcodeInit() {

  // Initializing random seed.
  initializeRandomSeed(); 
}

#pragma endregion VEXcode Generated Robot Configuration

// ============================================================
//  HARDWARE
// ============================================================

motor    motorXA(PORT12, false);    // left  X motor
motor    motorXB(PORT7,  true);     // right X motor (reversed)
motor    motorY(PORT3,   false);    // Y axis
motor    motorBarrel(PORT4, false); // icing barrel rotation
motor    motorSyringe(PORT5, false);// syringe plunger (+ = squeeze)
motor    motorPlate(PORT10, false); // cookie plate rotation (calibration)

bumper   bumperX(PORT1);
bumper   bumperY(PORT2);
touchled startButton(PORT6);
optical  barrelOptical(PORT11);     // optical sensor aimed at active syringe
distance distSensor(PORT8);        // distance sensor on nozzle carriage (calibration)

// ============================================================
//  MECHANICAL CONSTANTS
// ============================================================

const double COUNTS_PER_INCH = 235.3; // THIS NEEDS SO MUCH TUNING HOLY SHIT

const double GANTRY_MIN      = 0.0;
const double GANTRY_MAX      = 8.0;

const double SEC_GRID_RADIUS = 12.0;

// Writable so calibrateCookie() can update them
double MAIN_CENTER_X = 4.0;
double MAIN_CENTER_Y = 4.0;

bool alreadyStarted = true;

// ============================================================
//  BARREL / COLOR CONSTANTS
// ============================================================

const int    NUM_COLORS        = 3;
const double BARREL_STEP_DEG   = 120.0;
const int    BARREL_SCAN_SPEED = 5;
const int    BARREL_MOVE_SPEED = 15;

const double SYRINGE_PRELOAD_DEG = 15.0;  //unused unless needed

// Hue windows for each color slot (depends a LOT on lighting)
// Slot 0: blue   hue ~230
// Slot 1: red    hue ~352
// Slot 2: yellow hue ~42
const int COLOR_HUE_CENTER[NUM_COLORS] = { 230, 352,  42 };
const int HUE_WINDOW_LO[NUM_COLORS]    = { 225, 345,  35 };
const int HUE_WINDOW_HI[NUM_COLORS]    = { 235, 360,  45 };
const int HUE_TOLERANCE                = 80;

// ============================================================
//  SYRINGE CONSTANTS
// ============================================================

const int    SYRINGE_SPEED_LINE = 3; // tune this, finicky ass variable
const int    SYRINGE_SPEED_DOT  = 6;
const int    SYRINGE_DOT_MS     = 700;
const double SYRINGE_FULL_DEG   = 525.0;

// ============================================================
//  MOTION CONSTANTS
// ============================================================

const int    GANTRY_SPEED  = 50;
const int    HOME_SPEED    = 25;
const int    LINE_SPEED    = 40;
const int    ZERO_INTERVAL = 5;

const double TUNE_X = 1.0;   // scale X motor output (probably leave alone)
const double TUNE_Y = 0.91;  // scale Y motor output, Y overshoots so tune under

// ============================================================
//  CALIBRATION CONSTANTS
// ============================================================

const int    SCAN_SPEED      = 15;    // slow sweep speed during prong search
const int    PRONG_DETECT_MM = 30;     // distance sensor threshold (mm)
const double SEARCH_RANGE    = 3.0;   // inches either side of centre to search
const double PRONG_Y_STEP = 0.9;  // vertical distance A to BC row in inches — tune this

// ============================================================
//  PASTE BLOCK  -  replace everything between the arrows with
//  the output from frontend, then download
//
//  cookieDiameter = number in INCHES please;
//
//  const double DIRECTIONS[] = { paste from frontend };
//  const int    COLORS[]     = { paste from frontend };
//
//  Do NOT change variable names, or our thing's going to crap itself.
// ============================================================

double cookieDiameter = 4.0; // adjust based on your diameter

const double DIRECTIONS[] = {}; // paste from frontend

const int COLORS[] = {}; // paste from frontend

// PASTE END
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// ============================================================
//  RUNTIME STATE
// ============================================================

double currentX = 0.0;
double currentY = 0.0;

double barrelColorPos[NUM_COLORS] = { 0.0, 0.0, 0.0 };
int    barrelColorHue[NUM_COLORS] = { 0,   0,   0   };
int    activeColorIndex = -1;

double plungerHeight[NUM_COLORS] = { 0.0, 0.0, 0.0 };

double syringeDist[NUM_COLORS] = {
  SYRINGE_FULL_DEG,
  SYRINGE_FULL_DEG,
  SYRINGE_FULL_DEG
};

int functionCounter = 0;

// ============================================================
//  HELPERS: unit conversions
// ============================================================

double inchesToDegX(double inches) { return inches * COUNTS_PER_INCH * TUNE_X; }
double inchesToDegY(double inches) { return inches * COUNTS_PER_INCH * TUNE_Y; }
double degToInches(double deg)     { return deg / COUNTS_PER_INCH; }

// ============================================================
//  HELPERS: secondary grid -> main grid
// ============================================================

double secToMainX(double sx) {
  double scale = (cookieDiameter / 2.0) / SEC_GRID_RADIUS;
  return sx * scale + MAIN_CENTER_X;
}

double secToMainY(double sy) {
  double scale = (cookieDiameter / 2.0) / SEC_GRID_RADIUS;
  return sy * scale + MAIN_CENTER_Y;
}

double clampMain(double v) {
  if (v < GANTRY_MIN) return GANTRY_MIN;
  if (v > GANTRY_MAX) return GANTRY_MAX;
  return v;
}

// ============================================================
//  HELPERS: syringe capacity check
// ============================================================

void syringeCheck(int slot) {
  if (syringeDist[slot] <= 0) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    const char* names[NUM_COLORS] = { "Blue", "Red", "Yellow" };
    Brain.Screen.print("%s syringe empty!", slot < NUM_COLORS ? names[slot] : "?");
    wait(1000, msec);
  }
}

// ============================================================
//  ZEROING
// ============================================================

void zeroGantry() {
  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Zeroing...");

  motorXA.setVelocity(HOME_SPEED, percent);
  motorXB.setVelocity(HOME_SPEED, percent);
  motorY.setVelocity(HOME_SPEED, percent);

  motorXA.spin(reverse);
  motorXB.spin(reverse);
  motorY.spin(reverse);

  bool xDone = false;
  bool yDone = false;

  while (!xDone || !yDone) {
    if (!xDone && bumperX.pressing()) {
      motorXA.stop(brake);
      motorXB.stop(brake);
      xDone = true;
      Brain.Screen.setCursor(2, 1);
      Brain.Screen.print("X zeroed");
    }
    if (!yDone && bumperY.pressing()) {
      motorY.stop(brake);
      yDone = true;
      Brain.Screen.setCursor(3, 1);
      Brain.Screen.print("Y zeroed");
    }
    wait(10, msec);
  }

  motorXA.resetPosition();
  motorXB.resetPosition();
  motorY.resetPosition();

  currentX = 0.0;
  currentY = 0.0;

  Brain.Screen.clearScreen();
}

// ============================================================
//  MOTION: both axes simultaneously
// ============================================================

void moveToMain(double targetX, double targetY, int speed = GANTRY_SPEED) {
  targetX = clampMain(targetX);
  targetY = clampMain(targetY);

  double degX = inchesToDegX(targetX);
  double degY = inchesToDegY(targetY);

  motorXA.setVelocity(speed, percent);
  motorXB.setVelocity(speed, percent);
  motorY.setVelocity(speed, percent);

  motorXA.spinToPosition(degX, degrees, false);
  motorXB.spinToPosition(degX, degrees, false);
  motorY.spinToPosition(degY,  degrees, false);

  while (motorXA.isSpinning() || motorXB.isSpinning() || motorY.isSpinning()) {
    wait(10, msec);
  }

  currentX = targetX;
  currentY = targetY;
}

// ============================================================
//  MOTION: X axis only
// ============================================================

void moveXOnly(double targetX, int speed = GANTRY_SPEED) {
  targetX = clampMain(targetX);

  motorXA.setVelocity(speed, percent);
  motorXB.setVelocity(speed, percent);

  motorXA.spinToPosition(inchesToDegX(targetX), degrees, false);
  motorXB.spinToPosition(inchesToDegX(targetX), degrees, false);

  while (motorXA.isSpinning() || motorXB.isSpinning()) {
    wait(10, msec);
  }

  currentX = targetX;
}

// ============================================================
//  MOTION: Y axis only
// ============================================================

void moveYOnly(double targetY, int speed = GANTRY_SPEED) {
  targetY = clampMain(targetY);

  motorY.setVelocity(speed, percent);
  motorY.spinToPosition(inchesToDegY(targetY), degrees, false);

  while (motorY.isSpinning()) {
    wait(10, msec);
  }

  currentY = targetY;
}

// ============================================================
//  MOTION: secondary grid coords
// ============================================================

void moveToSec(double sx, double sy, int speed = GANTRY_SPEED) {
  moveToMain(secToMainX(sx), secToMainY(sy), speed);
}

// ============================================================
//  BARREL: rotate to encoder position
// ============================================================

void rotateBarrelTo(double targetDeg) {
  motorBarrel.setVelocity(BARREL_MOVE_SPEED, percent);
  motorBarrel.spinToPosition(targetDeg, degrees, true);
}

// ============================================================
//  BARREL: nudge barrel encoder back to 0 like trevor said
// ============================================================

void zeroBarrel() {
  double pos = motorBarrel.position(degrees);
  motorBarrel.setVelocity(BARREL_SCAN_SPEED, percent);
  if (pos < 0) {
    motorBarrel.spin(forward);
    while (motorBarrel.position(degrees) <= 0) {}
    motorBarrel.stop(brake);
  } else if (pos > 0) {
    motorBarrel.spin(reverse);
    while (motorBarrel.position(degrees) >= 0) {}
    motorBarrel.stop(brake);
  }
}

// ============================================================
//  BARREL: startup color scan
//
//  Slowly spins the barrel a full 360 degrees (any faster and
//  it will shit itself). Watches the optical sensor and records
//  the encoder position for each of the 3 color windows.
// ============================================================

void indexBarrelColors() {
  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Indexing colors...");

  barrelOptical.setLight(ledState::on);
  barrelOptical.setLightPower(100);
  motorBarrel.resetPosition();

  bool hueFound[NUM_COLORS] = { false, false, false };

  motorBarrel.setVelocity(BARREL_SCAN_SPEED, percent);
  motorBarrel.spin(forward);

  while (motorBarrel.position(degrees) <= 360) {
    int h = barrelOptical.hue();

    for (int i = 0; i < NUM_COLORS; i++) {
      if (!hueFound[i] &&
          h >= HUE_WINDOW_LO[i] &&
          h <= HUE_WINDOW_HI[i]) {

        wait(200, msec);  // settle

        h = barrelOptical.hue();
        if (h >= HUE_WINDOW_LO[i] - 10 && h <= HUE_WINDOW_HI[i] + 10) {
          barrelColorPos[i] = motorBarrel.position(degrees) - 10.0;
          barrelColorHue[i] = h;
          hueFound[i] = true;

          Brain.Screen.clearScreen();
          Brain.Screen.setCursor(1, 1);
          Brain.Screen.print("Slot %d hue=%d @ %.1f", i, h, barrelColorPos[i]);
          wait(500, msec);
        }
      }
    }
  }

  motorBarrel.stop(brake);

  for (int i = 0; i < NUM_COLORS; i++) {
    if (!hueFound[i]) {
      Brain.Screen.clearScreen();
      Brain.Screen.setCursor(1, 1);
      Brain.Screen.print("WARNING: slot %d not found", i);
      wait(1000, msec);
    }
  }

  zeroBarrel();
  // Do NOT pre-rotate or set activeColorIndex here;
  // leave activeColorIndex = -1 so selectColor() moves on first command.

  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Color index done");
  wait(500, msec);
  Brain.Screen.clearScreen();
}

// ============================================================
//  BARREL: find slot matching a requested hue
// ============================================================

int findColorSlot(int requestedHue) {
  int bestSlot = -1;
  int bestDiff = 9999;

  for (int i = 0; i < NUM_COLORS; i++) {
    int diff = abs(barrelColorHue[i] - requestedHue);
    if (diff > 180) diff = 360 - diff;
    if (diff < HUE_TOLERANCE && diff < bestDiff) {
      bestDiff = diff;
      bestSlot = i;
    }
  }

  Brain.Screen.clearScreen();
  Brain.Screen.print("%d", bestSlot);
  return bestSlot;
}

// ============================================================
//  BARREL: rotate to slot matching a given hue
// ============================================================

void syringePreload(int angle) {
  motorSyringe.setVelocity(SYRINGE_SPEED_LINE, percent);
  motorSyringe.spinFor(forward, angle, degrees, true);
}

void selectColor(int requestedHue, double (&heights)[NUM_COLORS]) {
  int slot = findColorSlot(requestedHue);
  if (slot == -1) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("Color not found! hue=%d", requestedHue);
    wait(1000, msec);
    return;
  }
  if (slot == activeColorIndex) return;

  if (activeColorIndex >= 0) {
    heights[activeColorIndex] = motorSyringe.position(degrees);
  }

  motorSyringe.spin(reverse, 15, percent);
  while (motorSyringe.position(degrees) >= 0) {}
  motorSyringe.stop(brake);

  rotateBarrelTo(barrelColorPos[slot]);

  activeColorIndex = slot;

  motorSyringe.setVelocity(SYRINGE_SPEED_LINE, percent);
  motorSyringe.spinToPosition(heights[activeColorIndex], degrees, true);

}

// ============================================================
//  SYRINGE: continuous squeeze (line drawing)
// ============================================================


void syringeStart() {  
  motorSyringe.setVelocity(SYRINGE_SPEED_LINE, percent);
  motorSyringe.spin(forward);
  wait(30, msec);
}

// ============================================================
//  SYRINGE: stop, save plunger position, update remaining
// ============================================================

void syringeStop() {
  motorSyringe.stop(hold);

  if (activeColorIndex >= 0) {
    double newPos    = motorSyringe.position(degrees);
    double dispensed = newPos - plungerHeight[activeColorIndex];
    plungerHeight[activeColorIndex] = newPos;
    syringeDist[activeColorIndex]  -= dispensed;
    syringeCheck(activeColorIndex);
  }
}

// ============================================================
//  SYRINGE: fixed dot dispense
// ============================================================

void syringeDot() {
  motorSyringe.setVelocity(SYRINGE_SPEED_DOT, percent);
  motorSyringe.spin(forward);
  wait(SYRINGE_DOT_MS, msec);
  motorSyringe.stop(hold);
  if (activeColorIndex >= 0) {
    double newPos    = motorSyringe.position(degrees);
    double dispensed = newPos - plungerHeight[activeColorIndex];
    plungerHeight[activeColorIndex] = newPos;
    syringeDist[activeColorIndex]  -= dispensed;
    syringeCheck(activeColorIndex);
  }
}

// ============================================================
//  SYRINGE: retract at end (irritating to reset after every run)
// ============================================================

void finalRetract(double (&heights)[NUM_COLORS]) {
  if (activeColorIndex < 0) {
    rotateBarrelTo(barrelColorPos[0]);
    activeColorIndex = 0;
  }
  motorSyringe.setVelocity(50, percent);
  motorSyringe.spinToPosition(0.0, degrees, true);
  motorSyringe.stop(hold);
}

// ============================================================
//  COUNTER: periodic re-zeroing
// ============================================================

void incrementAndCheckCounter() {
  functionCounter++;
  if (functionCounter >= ZERO_INTERVAL) {
    functionCounter = 0;
    zeroGantry();
  }
}

// ============================================================
//  DRAW LINE
// ============================================================

void drawLine(double x1, double y1, double x2, double y2, double (&heights)[NUM_COLORS]) {

  double mx1 = clampMain(secToMainX(x1));
  double my1 = clampMain(secToMainY(y1));
  double mx2 = clampMain(secToMainX(x2));
  double my2 = clampMain(secToMainY(y2));

  wait(1500, msec);

  moveToMain(mx1, my1, GANTRY_SPEED);
  if (alreadyStarted){
    syringePreload(30);
    alreadyStarted = false;
  }
  else{
    syringePreload(20);
  }
  
  syringeStart();

  motorXA.setVelocity(LINE_SPEED, percent);
  motorXB.setVelocity(LINE_SPEED, percent);
  motorY.setVelocity(LINE_SPEED, percent);

  motorXA.spinToPosition(inchesToDegX(mx2), degrees, false);
  motorXB.spinToPosition(inchesToDegX(mx2), degrees, false);
  motorY.spinToPosition(inchesToDegY(my2),  degrees, false);

  while (motorXA.isSpinning() || motorXB.isSpinning() || motorY.isSpinning()) {
    wait(10, msec);
  }

  currentX = mx2;
  currentY = my2;

  syringeStop();
  incrementAndCheckCounter();
}

// ============================================================
//  DRAW POINT
// ============================================================

void drawPoint(double x1, double y1, double (&heights)[NUM_COLORS]) {
  double mx = clampMain(secToMainX(x1));
  double my = clampMain(secToMainY(y1));

  moveToMain(mx, my, GANTRY_SPEED);
  wait(1500, msec);

  if (alreadyStarted){
    syringePreload(30);
    alreadyStarted = false;
  }
  else{
    syringePreload(20);
  }

  syringeDot();
  wait(1000, msec);
  incrementAndCheckCounter();
}

// ============================================================
//  EXECUTE PLAN
// ============================================================

void executePlan(const double directions[], int dirSize,
                 const int    colors[],     int colorSize,
                 double (&heights)[NUM_COLORS]) {
  int dirIdx = 0;
  int cmdIdx = 0;

  while (dirIdx < dirSize) {
    int cmd = (int)directions[dirIdx++];

    if (cmdIdx < colorSize) {
      wait(50, msec);
      selectColor(colors[cmdIdx], heights);
    }

    if (cmd == 1) {
      if (dirIdx + 4 > dirSize) break;
      double x1 = directions[dirIdx++];
      double y1 = directions[dirIdx++];
      double x2 = directions[dirIdx++];
      double y2 = directions[dirIdx++];
      drawLine(x1, y1, x2, y2, heights);

    } else if (cmd == 2) {
      if (dirIdx + 2 > dirSize) break;
      double x1 = directions[dirIdx++];
      double y1 = directions[dirIdx++];
      drawPoint(x1, y1, heights);

    } else {
      Brain.Screen.clearScreen();
      Brain.Screen.setCursor(1, 1);
      Brain.Screen.print("Cmd ERROR: %d", cmd);
      wait(500, msec);
    }

    cmdIdx++;
  }
}

// ============================================================
//  CALIBRATION
// ============================================================

double sweepXForProng(double startX, double endX) {
  moveXOnly(startX, GANTRY_SPEED);

  motorXA.setVelocity(SCAN_SPEED, percent);
  motorXB.setVelocity(SCAN_SPEED, percent);

  motorXA.spinToPosition(inchesToDegX(clampMain(endX)), degrees, false);
  motorXB.spinToPosition(inchesToDegX(clampMain(endX)), degrees, false);

  while (motorXA.isSpinning()) {
    if (distSensor.objectDistance(mm) <= PRONG_DETECT_MM) {
      motorXA.stop(brake);
      motorXB.stop(brake);
      wait(20, msec);
      return degToInches(motorXA.position(degrees));
    }
    wait(5, msec);
  }
  return -1.0;
}

double sweepYForProng(double startY, double endY) {
  moveYOnly(startY, 20);
  wait(300, msec);

  double targetDeg = inchesToDegY(clampMain(endY));

  motorY.setVelocity(5, percent);
  motorY.spinToPosition(targetDeg, degrees, false);

  wait(100, msec);

  while (true) {
    int dist = distSensor.objectDistance(mm);
    double currentDeg = motorY.position(degrees);

    Brain.Screen.setCursor(2, 1);
    Brain.Screen.print("Dist: %d mm   ", dist);
    Brain.Screen.setCursor(3, 1);
    Brain.Screen.print("Y: %.2f in   ", degToInches(currentDeg));

    if (dist > 0 && dist <= 70) {
      motorY.stop(brake);
      wait(20, msec);
      return degToInches(motorY.position(degrees));
    }

    if (fabs(currentDeg - targetDeg) < 20) {
      return -1.0;
    }

    wait(50, msec);
  }
}

void calibrateCookie() {
  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Calibrating...");

  Brain.Screen.setCursor(2, 1);
  Brain.Screen.print("Step 1: sweep Y -> A");

  moveXOnly(MAIN_CENTER_X, GANTRY_SPEED);
  moveYOnly(MAIN_CENTER_Y, GANTRY_SPEED);

  double yA = sweepYForProng(GANTRY_MAX, GANTRY_MIN);

  if (yA < 0) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("ERROR: A not found");
    wait(2000, msec);
    return;
  }

  double xA = MAIN_CENTER_X;
  Brain.Screen.setCursor(3, 1);
  Brain.Screen.print("A: (%.2f, %.2f)", xA, yA);
  wait(500, msec);

  Brain.Screen.setCursor(2, 1);
  Brain.Screen.print("Step 2: sweep X -> B");

  double yBC = yA - PRONG_Y_STEP;
  moveYOnly(yBC, GANTRY_SPEED);

  double xB = sweepXForProng(
    MAIN_CENTER_X + SEARCH_RANGE,
    MAIN_CENTER_X - SEARCH_RANGE
  );

  if (xB < 0) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("ERROR: B not found");
    wait(2000, msec);
    return;
  }

  Brain.Screen.setCursor(3, 1);
  Brain.Screen.print("B: (%.2f, %.2f)", xB, yBC);
  wait(500, msec);

  Brain.Screen.setCursor(2, 1);
  Brain.Screen.print("Step 3: sweep X -> C");

  double xC = sweepXForProng(
    xB + 0.5,
    MAIN_CENTER_X + SEARCH_RANGE
  );

  if (xC < 0) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("ERROR: C not found");
    wait(2000, msec);
    return;
  }

  Brain.Screen.setCursor(3, 1);
  Brain.Screen.print("C: (%.2f, %.2f)", xC, yBC);
  wait(500, msec);

  double ax = xA,  ay = yA;
  double bx = xB,  by = yBC;
  double cx = xC,  cy = yBC;

  double D = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

  if (fabs(D) < 1e-6) {
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("ERROR: points collinear");
    wait(2000, msec);
    return;
  }

  double ux = ((ax*ax + ay*ay) * (by - cy) +
               (bx*bx + by*by) * (cy - ay) +
               (cx*cx + cy*cy) * (ay - by)) / D;

  double uy = ((ax*ax + ay*ay) * (cx - bx) +
               (bx*bx + by*by) * (ax - cx) +
               (cx*cx + cy*cy) * (bx - ax)) / D;

  double R = sqrt((ax - ux)*(ax - ux) + (ay - uy)*(ay - uy));

  cookieDiameter = 2.0 * R;
  MAIN_CENTER_X  = ux;
  MAIN_CENTER_Y  = uy;

  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Done!");
  Brain.Screen.setCursor(2, 1);
  Brain.Screen.print("Diam: %.2f in", cookieDiameter);
  Brain.Screen.setCursor(3, 1);
  Brain.Screen.print("Ctr: (%.2f,%.2f)", MAIN_CENTER_X, MAIN_CENTER_Y);
  Brain.Screen.setCursor(4, 1);
  Brain.Screen.print("Press start...");

  waitUntil(startButton.pressing());
  wait(200, msec);
  Brain.Screen.clearScreen();
}

// ============================================================
//  ENTRY POINT
// ============================================================

int main(){
  vexcodeInit();

  Brain.playNote(4, 0, 150);
  wait(30, msec);
  Brain.playNote(4, 2, 150);
  wait(30, msec);
  Brain.playNote(4, 4, 150);
  wait(30, msec);
  Brain.playNote(5, 0, 400);

  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Press start...");
  waitUntil(startButton.pressing());
  wait(200, msec);
  Brain.Screen.clearScreen();

  int dirSize   = sizeof(DIRECTIONS) / sizeof(DIRECTIONS[0]);
  int colorSize = sizeof(COLORS)     / sizeof(COLORS[0]);

  zeroGantry();

  //calibrateCookie();
  moveToSec(0.0, 0.0);
  wait(200, msec);
  indexBarrelColors();
  executePlan(DIRECTIONS, dirSize, COLORS, colorSize, plungerHeight);
  moveToSec(0.0, 0.0);
  zeroGantry();
  finalRetract(plungerHeight);

  Brain.Screen.clearScreen();
  Brain.Screen.setCursor(1, 1);
  Brain.Screen.print("Done!");

  Brain.playNote(4, 0, 150);
  wait(30, msec);
  Brain.playNote(4, 2, 150);
  wait(30, msec);
  Brain.playNote(4, 4, 150);
  wait(30, msec);
  Brain.playNote(5, 0, 400);

  return EXIT_SUCCESS;
}