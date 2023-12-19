#include "LedControl.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>
// Pin definitions for the LED matrix
const int dinPin = 12;
const int clockPin = 11;
const int loadPin = 10;
 
// Pin definitions for the joystick
const int xPin = A0;
const int yPin = A1;
const int swPin = A2;


// lcd variables
const byte rs = 9;
const byte en = 8;
const byte d4 = 7;
const byte d5 = 6;
const byte d6 = 5;
const byte d7 = 4;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Pins for brightness
const int backlightPin = 3;
const int potentiometerPin = A3;


// Custom characters
byte heartIcon[8] = { B00000, B00000, B01010, B11111, B11111, B01110, B00100, B00000 };
byte fruitIcon[8] = { B00000, B00010, B00100, B01110, B10111, B11111, B01110, B00000 };
byte arrowIcon[8] = { B00000, B00000, B01000, B01100, B01110, B01100, B01000, B00000 };
byte blockIcon[8] = { B11111, B11001, B11101, B11101, B11111, B11111, B11111, B11111 };

// --------------------------------------------------------------------

// Buzzer variables
struct BuzzerNote {
  int frequency;
  int duration;
};

BuzzerNote introMelody[] = {
  {261, 200},
  {329, 200},
  {392, 200},
  {523, 200}
};

int buzzerCurrent = 0;
unsigned long noteEndTime = 0;

const int buzzerPin = 2;
int buzzerTone = 1000;
int buzzerTime = 50;
int buzzerState = 0;
 
// Thresholds and directions for joystick movement detection
const int minThreshold = 400;
const int maxThreshold = 600;
unsigned int readJoyStickDebounce = 300;
unsigned int lastReadJoystick = 0;
enum SwitchPosition {
  UP,
  DOWN,
  LEFT,
  RIGHT,
  CENTER,
  NONE
};
 
// Timing variables for movement updates
int moveInterval = 200;
int moveIntervalSnake = 200;  // tune this parameter for player speed
unsigned long lastMoved = 0;
unsigned long lastMovedSnake = 0;
 

// ----------------------------------------------------------

// LedControl object for matrix
LedControl lc = LedControl(dinPin, clockPin, loadPin, 1); // DIN, CLK, LOAD, number of devices
 
// Size of the LED matrix (8x8)
const byte matrixSize = 8;
bool matrixChanged = true;

// 2D array representing the state (on/off) of each LED in the matrix
byte matrix[matrixSize][matrixSize] = { /* Initial state with all LEDs off */ };

// Variable to control LED matrix brightness
byte brightness[2] = {2,2}; // 0 - lcd brightness, 1 - matrix brightness
int brightnessEepromAddress = 0;

// Variable to control sounds
int soundEepromAddress = 2;
byte soundState = HIGH;
 

// --------------------------------------------------------------------

// Menu variables
int gameState = 1; // 0 while playing, others while viewing menu
/*
  0 - currently playing; showing live score
  1 - viewing main menu
  2 - viewing settings menu
  3 - showing game info
  4 - after death display
  5 - leaderboard
*/

// delays for info page
int infoDelayTime = 0;
int infoDelayInterval = 1500;

String mainMenuItems[] = {"Start!", "Settings", "About the game"};
String settingsMenuItems[] = {"Menu light", "Game light", "Sounds", "Go back"};
int mainMenuCurrentItem = 0; // current item displayed on menu
int settingsMenuCurrentItem = 0;

int bestScores[3] = {0, 0, 0};
int bestScoresNumber = 3;
int bestScoresEepromAddress = 3;
int scoreDisplayed = 0;

// --------------------------------------------------------------------

// Snake variables
int snakeSize = 3;
// Define a struct to represent a coordinate
struct Coordinate {
  int x;
  int y;
};
Coordinate snake[] = {{2, 2}, {1, 2}, {0, 2}}; // Create a vertical 3-piece snake at the left edge of the matrix
byte xLastPos = 2;
byte yLastPos = 2;

// Represent direction
struct Direction {
  int dx;
  int dy;
};
// Last movement direction (initialised to right)
Direction lastDirection = {1, 0};


// Fruit variables
Coordinate fruit;
bool fruitSpawned = false;
unsigned int fruitBlinkInterval = 100;
unsigned int fruitLastBlink = 0;
bool fruitOn = false;
unsigned int fruitsEaten = 0;

unsigned int remainingLives = 3;

// --------------------------------------------------------------------


void setup() {
  Serial.begin(9600);

  pinMode(swPin, INPUT_PULLUP);
  pinMode(backlightPin, OUTPUT);
  lc.shutdown(0, false); // Disable power saving, turn on the display

  // Create special characters
  lcd.createChar(0, heartIcon);
  lcd.createChar(1, fruitIcon);
  lcd.createChar(2, arrowIcon);
  lcd.createChar(3, blockIcon);


  // Set the brightness levels
  brightness[0] = EEPROM.read(brightnessEepromAddress);
  brightness[1] = EEPROM.read(brightnessEepromAddress +1);
  analogWrite(backlightPin, brightness[0]);
  lc.setIntensity(0, brightness[1]);

  // Set sound ON/OFF
  soundState = EEPROM.read(soundEepromAddress);

  // Set best scores
  bestScores[0] = EEPROM.read(bestScoresEepromAddress);
  bestScores[1] = EEPROM.read(bestScoresEepromAddress+1);
  bestScores[2] = EEPROM.read(bestScoresEepromAddress+2);
  
  startUp();
  lcd.clear();
  matrix[snake[0].x][snake[0].y] = 1; // Turn on the initial LED position

  randomSeed(analogRead(0)); // seeding from empty pin (noise) so that fruits are randomly generated
  generateFruit();
  analogWrite(backlightPin, brightness[0]);
}

void loop() {
  inMenu(gameState);
  if (gameState == 0) {
    inGame();
  }
}

// Intro when opening the game
void startUp() {
  // Display intro message on the LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("SNAKE!");
  lcd.setCursor(3, 1);
  lcd.print("welcome :)");

  // Light up the LEDs on the matrix in a spiral from the center to the border
  int startRow = 0, endRow = matrixSize - 1;
  int startCol = 0, endCol = matrixSize - 1;

  while (startRow <= endRow && startCol <= endCol) {
    for (int i = startCol; i <= endCol; i++) {
      lc.setLed(0, startRow, i, true);
      delay(20);
    }
    startRow++;

    for (int i = startRow; i <= endRow; i++) {
      lc.setLed(0, i, endCol, true);
      delay(20);
    }
    endCol--;

    if (startRow <= endRow) {
      for (int i = endCol; i >= startCol; i--) {
        lc.setLed(0, endRow, i, true);
        delay(20);
      }
      endRow--;
    }

    if (startCol <= endCol) {
      for (int i = endRow; i >= startRow; i--) {
        lc.setLed(0, i, startCol, true);
        delay(20);
      }
      startCol++;
    }
  }
  buzzerIntro();
}

// ---------------------------------------------------------------

// States of the game
void inMenu(int state) {
  switch (state) {
    case 0:
      Serial.println("0. in game\n");
      gameStats();
      break;
    case 1:
      Serial.println("1. Main Menu\n");
      mainMenu();
      break;
    case 2:
      Serial.println("2. Settings Menu\n");
      settingsMenu();
      break;
    case 3:
      Serial.println("3. About the game\n");
      infoPage();
      break;
    case 4:
      Serial.println("4. After the game\n");
      afterGame();
      break;
    case 5:
      Serial.println("5. Leaderboard\n");
      leaderboard();
      break;
    default:
      Serial.println("Not an option\n");
      Serial.print("Tried ");
      Serial.println(state);
  }
}


void mainMenu() {
  // Update the display
  lcd.setCursor(0, 0);
  lcd.print("   MAIN MENU");
  lcd.setCursor(1, 1);
  lcd.write(byte(2));
  lcd.print(mainMenuItems[mainMenuCurrentItem]);

  // Read the joystick input
  SwitchPosition switchPosition = readSwitch();
    
  if (switchPosition == DOWN) {
    mainMenuCurrentItem--;
    if (mainMenuCurrentItem < 0){
      mainMenuCurrentItem = 2;
    }
  } else if (switchPosition == UP) {
    mainMenuCurrentItem = (mainMenuCurrentItem+1) % 3;
  } else if (switchPosition == CENTER) {
    mainMenuChoice(mainMenuCurrentItem);
  }

  if (switchPosition != NONE){
    lcd.clear();
    delay(20);
  }
  
}

void mainMenuChoice(int option){
  switch (option){
    case 0:
      // start game
      gameState = 0;
      lcd.clear();
      remainingLives = 3;
      fruitsEaten = 0;
      break;
    case 1:
      // settings menu
      gameState = 2;
      break;
    case 2:
      // about game
      gameState = 3;
      break;
    default:
      Serial.println("Not an option\n");
  }
}

void settingsMenu() {
  // Display the currently selected item
  lcd.setCursor(0, 0);
  lcd.print(settingsMenuItems[settingsMenuCurrentItem]);

  // Read the joystick input
  SwitchPosition switchPosition = readSwitch();

  if (switchPosition == DOWN) { // scroll down
    settingsMenuCurrentItem--;
    if (settingsMenuCurrentItem < 0){
      settingsMenuCurrentItem = 3;
    }
  } 
  else if (switchPosition == UP) {  // scroll up
    settingsMenuCurrentItem = (settingsMenuCurrentItem+1) % 4;
  } 
  else if (switchPosition == CENTER && settingsMenuCurrentItem == 3) {  // go back to main menu
    gameState = 1;
  }
  else if (switchPosition == CENTER && settingsMenuCurrentItem == 2) {  // toggle volume
    soundState = !soundState;
    EEPROM.update(soundEepromAddress, soundState);
  }

  // Update the display
  lcd.clear();
  lcd.setCursor(0, 0);

  lcd.print(settingsMenuItems[settingsMenuCurrentItem]);
  lcd.setCursor(0, 1);



  if (settingsMenuCurrentItem == 0){ // show lcd brightness
    int potValue = analogRead(potentiometerPin);
    brightness[0] = map(potValue, 0, 1023, 0, 255);
    analogWrite(backlightPin, brightness[0]);
    
    lcd.print(" Intensity:");
    displayIntensity(brightness[0], 70, 150);

    brightnessToEeprom();
  }

  if (settingsMenuCurrentItem == 1){ // show matrix brightness
    int potValue = analogRead(potentiometerPin);
    brightness[1] = map(potValue, 0, 1023, 0, 15);
    lc.setIntensity(0, brightness[1]);
    
    lcd.print(" Intensity:");
    displayIntensity(brightness[1], 0, 10);

    brightnessToEeprom();
  }

  if (settingsMenuCurrentItem == 2){ // turn sounds on/off
    lcd.print(" Turned ");
    if (soundState){
      lcd.print("ON");
    }
    else{
      lcd.print("OFF");
    }
    
  }

  // }
}

void gameStats(){
  analogWrite(backlightPin, brightness[0]);
  lcd.clear();
  // display eaten fruits
  displayFruits();
  // display current lives
  displayLives();
}

void  infoPage(){
  int scrollDebounce = millis() - infoDelayTime;
  lcd.clear();
  if (scrollDebounce < infoDelayInterval) {
    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Creator: ");
    lcd.setCursor(0, 1);
    lcd.print(" Cristina Damov ");
  }
  else if(scrollDebounce >= infoDelayInterval && scrollDebounce < infoDelayInterval * 2) {
    lcd.setCursor(0, 0);
    lcd.print(" Github: ");
    lcd.setCursor(0, 1);
    lcd.print(" criss505 ");
  }
  else if (scrollDebounce >= infoDelayInterval * 2 && scrollDebounce < infoDelayInterval * 3) {
    // lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Game: ");
    lcd.setCursor(0, 1);
    lcd.print(" snake! ");
  } else {
    infoDelayTime = millis();
  }

  // Go back to main menu when doing any joystick movement
  SwitchPosition joystickReading = readSwitch();
  if (joystickReading != NONE) {
    gameState = 1;
  }

}

void afterGame(){
  analogWrite(backlightPin, brightness[0]);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("you died! xp");

  lcd.setCursor(0, 1);
  if (fruitsEaten < bestScores[2]){
    lcd.print("try again!");
  }
  else {
    lcd.print("good score!");
  }

  // Go back to main menu when doing any joystick movement
  SwitchPosition joystickReading = readSwitch();
  if (joystickReading != NONE) {
    if (fruitsEaten > bestScores[2]){ // add new score to leaderboard if needed
      addBestScore();
    }
    gameState = 5;
    scoreDisplayed = 0;
    lcd.clear();
    
  }
}

void leaderboard(){
  analogWrite(backlightPin, brightness[0]);

  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("leaderboard");
  lcd.setCursor(0, 1);
  lcd.print(scoreDisplayed + 1);
  lcd.print(". ");
  lcd.print(bestScores[scoreDisplayed]);
  lcd.print(" fruits");
  
  // Scroll through best scores
  SwitchPosition joystickReading = readSwitch();
  if (joystickReading == DOWN) {
    scoreDisplayed--;
    if (scoreDisplayed < 0) {
      scoreDisplayed = 2;
    }
  }
  else if (joystickReading == UP) {
    scoreDisplayed = (scoreDisplayed + 1) % 3;
  }
  // Go back to main menu
  if (joystickReading == CENTER) {
    gameState = 1;
    lcd.clear();
  }

}


// ---------------------------------------------------------------

// Update new brightness variables in eeprom
void brightnessToEeprom(){ 
  EEPROM.update(brightnessEepromAddress, brightness[0]);
  EEPROM.update(brightnessEepromAddress +1, brightness[1]);
}

// Reads joystick state
SwitchPosition readSwitch(){
  // Reading debounce delay
  if (millis() - lastMoved > moveInterval) {
    // Read X-axis and Y-axis values
    int xValue = analogRead(xPin);
    int yValue = analogRead(yPin);
    int swValue = digitalRead(swPin);

    if (xValue > minThreshold && xValue < maxThreshold && yValue > minThreshold && yValue < maxThreshold && swValue == HIGH){
      return NONE;
    }
    // checked the none condition to not reset the lastMoved
    lastMoved = millis(); // Reset the movement timer

    if (xValue < minThreshold) return DOWN;
    if (xValue > maxThreshold) return UP;
    if (yValue < minThreshold) return LEFT;
    if (yValue > maxThreshold) return RIGHT;
    if (swValue == LOW) return CENTER;

  }
    return NONE;
}

// ---------------------------------------------------------------

// Loop while playing the game
void inGame(){
  // Display snake body parts
  for (int i = 0; i < snakeSize; i++) {
    lc.setLed(0, snake[i].x, snake[i].y, true);
  }

  blinkFruit();

  if (millis() - lastMovedSnake > moveIntervalSnake){
    updatePositions(); // Update the LED position based on joystick input
    checkMatrixBounds();
    checkEatFruit();

    lastMovedSnake = millis();
  }
  updateMatrix();
}
 

// Update the LED matrix display
void updateMatrix() {
  for (int row = 0; row < matrixSize; row++) {
    for (int col = 0; col < matrixSize; col++) {
      lc.setLed(0, row, col, matrix[row][col]);
    }
  }
}
 
// Read joystick input and update LED positions accordingly
void updatePositions() {
  // Store the last positions
  xLastPos = snake[0].x;
  yLastPos = snake[0].y;

  // Shift snake
  for (int i = snakeSize-1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }

  SwitchPosition joystickPosition = readSwitch();
  bool changeDirection = false;

  // Update xPos based on joystick movement
  if (lastDirection.dy != 0){ // Doesn't make sense to change direction to the face or back of the snake, only left/right
    if (joystickPosition == DOWN) {
      snake[0].x = snake[0].x + 1;
      lastDirection = {1, 0};
      changeDirection = true;
    } else if (joystickPosition == UP) {
      snake[0].x = snake[0].x - 1;
      lastDirection = {-1, 0};
      changeDirection = true;
    } 
    checkMatrixBounds();
  }

  // Update yPos based on joystick movement
  if (lastDirection.dx != 0){ // && snake[1].x == snake[0].x) {
    if (joystickPosition == LEFT && lastDirection.dy != 1) {
      snake[0].y = snake[0].y + 1;
      lastDirection = {0, 1};
      changeDirection = true;
    } else if (joystickPosition == RIGHT && lastDirection.dy != -1) {
      snake[0].y = snake[0].y - 1;
      lastDirection = {0, -1};
      changeDirection = true;
      checkMatrixBounds();
    }
  }

  if (changeDirection != true) {
    snake[0].x = snake[0].x + lastDirection.dx;
    snake[0].y = snake[0].y + lastDirection.dy;
  }
   
  checkMatrixBounds();
 

  // Check if the position has changed and update the matrix accordingly
  
  if ((snake[0].x != xLastPos || snake[0].y != yLastPos)) {
    matrix[xLastPos][yLastPos] = 0; // Turn off the LED at the last position
    matrix[snake[0].x][snake[0].y] = 1; // Turn on the LED at the new position
  }
}
 
// Check if player encounters walls
void checkMatrixBounds() {
  if (snake[0].x < 0 || snake[0].x >= matrixSize || snake[0].y < 0 || snake[0].y >= matrixSize) { // Snake hit a wall
    remainingLives--;
    buzzerHitWall();
    if (remainingLives == 0){
      gameState = 4;
      buzzerDeath();
    }
    
    // Put snake back on map for next life
    snake[0].x = (snake[0].x + matrixSize) % matrixSize;  
    snake[0].y = (snake[0].y + matrixSize) % matrixSize;
  }
}

// Random fruit generator
void generateFruit() {
  bool touchSnake; // The fruit can't generate in a position occupied by snake

  if (fruitSpawned == false) {  // Only one fruit will be generated at a time
    do {
      touchSnake = false;

      // Generate a random position within the matrix
      fruit.x = random(matrixSize);
      fruit.y = random(matrixSize);

      // Check if the position coincides with any part of the snake
      for (int i = 0; i < 3; i++) {
        if (snake[i].x == fruit.x && snake[i].y == fruit.y) {
          touchSnake = true;
          break;
        }
      }
      fruitSpawned = true; // fruit generated succesfully!
      Serial.println("Fruit generated");
    } while (touchSnake);
  }
}

// Make the fruit blink on the matrix
void blinkFruit(){
  if (fruitSpawned) {
    if (millis() - fruitLastBlink >= fruitBlinkInterval) {
      fruitOn = !fruitOn; // Toggle the fruit's LED
      matrix[fruit.x][fruit.y] = fruitOn ? 1 : 0; // Update the matrix
      fruitLastBlink = millis(); // Update the last blink time
    }
  }
}

// Check if player encounters fruit
void checkEatFruit(){
  if (snake[0].x == fruit.x && snake[0].y == fruit.y) {
    // Serial.println("Fruit eaten");
    fruitsEaten++;
    moveIntervalSnake -= 5;
    buzzerEatFruit();

    fruitSpawned = false;
    generateFruit();
  }
}

// Grow size of player
void growSnake() {
  snakeSize++;
  // Initialize the new tail segment based on the last movement direction
  snake[snakeSize - 1].x = snake[snakeSize - 2].x - lastDirection.dx;
  snake[snakeSize - 1].y = snake[snakeSize - 2].y - lastDirection.dy;
}

// Show current number of lives while playing
void displayLives(){
  lcd.setCursor(5, 1);
  for (int i = 0; i < remainingLives; i++){
    lcd.write(byte(0));
    lcd.print(" ");
  }
}

// Show current score while playing
void displayFruits(){
  lcd.setCursor(6, 0);
  lcd.write(byte(1));
  lcd.print("x");
  lcd.print(fruitsEaten);
}

// Show brightness intensity on a 5-block scale
void displayIntensity(int value, int lowThreshold, int highThreshold){
  int intensityScale = map(value, lowThreshold, highThreshold, 0, 5);
  for (int i = 0; i < intensityScale; i++){
    lcd.write(byte(3));
  }
}

// Add current score to leaderboard
void addBestScore(){
  bestScores[2] = fruitsEaten;

  // Sort the high scores
  for (int i = 0; i < bestScoresNumber - 1; i++) {
      for (int j = 0; j < bestScoresNumber - i - 1; j++) {
          if (bestScores[j] < bestScores[j + 1]) {
            // Swap bestScores[j] and bestScores[j + 1]
            int temp = bestScores[j];
            bestScores[j] = bestScores[j + 1];
            bestScores[j + 1] = temp;
          }
      }
  }

  // Save to eeprom the new high scores
  EEPROM.update(bestScoresEepromAddress, bestScores[0]);
  EEPROM.update(bestScoresEepromAddress+1, bestScores[1]);
  EEPROM.update(bestScoresEepromAddress+2, bestScores[2]);
}


// ----------------------------------------------------

const int NOTE_E3 = 165;
const int NOTE_G3 = 196;
const int NOTE_A3 = 220;
const int NOTE_B3 = 247;
const int NOTE_C4 = 261;
const int NOTE_E4 = 329;
const int NOTE_G4 = 392;
const int NOTE_C5 = 523;

// Small tune when opening game
void buzzerIntro() {
  int introMelody[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5}; // notes
  int introDuration[] = {200, 150, 200, 250}; // duration of each note
  if (soundState){

    for (int i = 0; i < 4; i++) {
      tone(buzzerPin, introMelody[i], introDuration[i]);
      delay(introDuration[i]); //delay between notes
    }
  }
}

// Play a sequence of notes for the death sound
void buzzerDeath() {
  int numberOfTones = 5;
  int deathMelody[numberOfTones] = {NOTE_A3, NOTE_G3, NOTE_E3, NOTE_B3, NOTE_C4}; // notes
  int deathDuration = 200;

  // Delay is ok because we also want a small pause after death
  if (soundState){
    for (int i = 0; i < numberOfTones; i++) {
      tone(buzzerPin, deathMelody[i], deathDuration);
      delay(deathDuration); //delay between notes
    }
  }
}

// Sound when eating a fruit
void buzzerEatFruit() {
  int eatFruitTone = 500;
  int duration = 150;
  if (soundState){
    tone(buzzerPin, eatFruitTone, duration);
  }
}

// Sound when hitting a wall
void buzzerHitWall() {
  int hitWallTone = 300;
  int duration = 150;
  if (soundState){
    tone(buzzerPin, hitWallTone, duration);
  }
}
