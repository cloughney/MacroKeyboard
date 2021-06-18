#include <Arduino.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <HID-Project.h>

// #define DEBUG

#define NUMBER_OF_KEYS          8
#define MAX_COMBINATION_KEYS    4

#define DEBOUNCING_MS          20
#define FIRST_REPEAT_CODE_MS  500
#define REPEAT_CODE_MS        150

#define ENCODER_CLK				A0
#define ENCODER_DT				A1
#define ENCODER_SW				A3

enum KeyState { Open, Debouncing, Pressed, Held };

struct Key {
	uint8_t pin;
	enum KeyState state;
	uint32_t stateStartMs;
	uint16_t keys[MAX_COMBINATION_KEYS];
};

Key keys[NUMBER_OF_KEYS] = {
	{ .pin =  6, .state = Open, .stateStartMs = 0, .keys = { KEY_F13 } },
	{ .pin =  7, .state = Open, .stateStartMs = 0, .keys = { KEY_F14 } },
	{ .pin =  8, .state = Open, .stateStartMs = 0, .keys = { KEY_F15 } },
	{ .pin =  9, .state = Open, .stateStartMs = 0, .keys = { KEY_F16 } },
	{ .pin = 15, .state = Open, .stateStartMs = 0, .keys = { KEY_F17 } },
	{ .pin = 14, .state = Open, .stateStartMs = 0, .keys = { KEY_F18 } },
	{ .pin =  5, .state = Open, .stateStartMs = 0, .keys = { KEY_F19 } },
	{ .pin = 10, .state = Open, .stateStartMs = 0, .keys = { KEY_F20 } }
}; 

ClickEncoder *encoder;
int16_t rotPosLast, rotPosCurrent;
bool applyDialModifier = false;

void timerIsr() { encoder->service(); }

bool stateExceedsThreshold(uint8_t keyIndex, uint16_t threshold) {
	return millis() - keys[keyIndex].stateStartMs > threshold;
}

void resetKeyState(uint8_t keyIndex) {
	Key* key = &keys[keyIndex];

	key->state = Open;
	key->stateStartMs = millis();
}

uint8_t triggerKey(uint8_t keyIndex) {
	Key *key = &keys[keyIndex];

	if (applyDialModifier) {
		Keyboard.press(KEY_LEFT_ALT);
	}

	for (uint8_t i = 0; i < MAX_COMBINATION_KEYS; i++) {
		if (!key->keys[i]) break;
		Keyboard.press((KeyboardKeycode)key->keys[i]);
	}

	for (uint8_t i = 0; i < MAX_COMBINATION_KEYS; i++) {
		if (!key->keys[i]) break;
		Keyboard.release((KeyboardKeycode)key->keys[i]);
	}

	if (applyDialModifier) {
		Keyboard.release(KEY_LEFT_ALT);
	}
}

void handleKeyPress() {
	for (uint8_t i = 0; i < NUMBER_OF_KEYS; i++) {
		bool pressed = digitalRead(keys[i].pin) == LOW;

		switch (keys[i].state) {

			case Open:
				if (pressed) {
					keys[i].state = Debouncing;
					keys[i].stateStartMs = millis();
					#ifdef DEBUG
					Serial.print("Key[Open -> Debouncing]: ");
					Serial.println(i);
					#endif
				}
				break;

			case Debouncing:
				if (!pressed) {
					resetKeyState(i);
				} else if (stateExceedsThreshold(i, DEBOUNCING_MS)) {
					keys[i].state = Pressed;
					#ifndef DEBUG
					triggerKey(i);
					#else
					Serial.print("Key[Debouncing -> Pressed]: ");
					Serial.println(i);
					#endif
				}
				break;
			
			case Pressed:
				if (!pressed) {
					resetKeyState(i);
				} else if (stateExceedsThreshold(i, FIRST_REPEAT_CODE_MS)) {
					keys[i].state = Held;
					keys[i].stateStartMs = millis();
					#ifndef DEBUG
					triggerKey(i);
					#else
					Serial.print("Key[Pressed -> Held]: ");
					Serial.println(i);
					#endif
				}
				break;

			case Held:
				if (!pressed) {
					resetKeyState(i);
				} else if (stateExceedsThreshold(i, REPEAT_CODE_MS)) {
					keys[i].stateStartMs = millis();
					#ifndef DEBUG
					triggerKey(i);
					#else
					Serial.print("Key[Held]: ");
					Serial.println(i);
					#endif
				}
				break;
		}
	}
}

void handleDialTurn() {
	rotPosCurrent += encoder->getValue();

	if (rotPosCurrent != rotPosLast) {
		uint16_t diff = abs(rotPosCurrent - rotPosLast);
		
		KeyboardKeycode key = (rotPosCurrent > rotPosLast) ? KEY_F24 : KEY_F23;
		for (uint8_t i = 0; i < diff; i++) {
			#ifndef DEBUG
			Keyboard.press(key);
			Keyboard.release(key);
			#else
			Serial.print("Dial turned ");
			Serial.println(rotPosCurrent - rotPosLast);
			#endif
		}

		rotPosLast = rotPosCurrent;
	}
}

void handleDialPress() {
	ClickEncoder::Button button = encoder->getButton();
	if (button != ClickEncoder::Open) {
		#ifndef DEBUG
		switch (button) {
			case ClickEncoder::Held:
				applyDialModifier = true;
				break;
			case ClickEncoder::Released:
				applyDialModifier = false;
				break;
			case ClickEncoder::Clicked:
				Keyboard.press(KEY_F21);
				Keyboard.release(KEY_F21);
				break;
			case ClickEncoder::DoubleClicked:
				Keyboard.press(KEY_F22);
				Keyboard.release(KEY_F22);
				break;
		}
		#else
		Serial.print("Button: ");
		#define VERBOSECASE(label) case label: Serial.println(#label); break;
		switch (button) {
			VERBOSECASE(ClickEncoder::Pressed);
			VERBOSECASE(ClickEncoder::Held)
			VERBOSECASE(ClickEncoder::Released)
			VERBOSECASE(ClickEncoder::Clicked)
			VERBOSECASE(ClickEncoder::DoubleClicked)
		}
		#endif
	}
}

void setup() {
	#ifdef DEBUG
	Serial.begin(9600);
	#endif

	Consumer.begin();
	Keyboard.begin();

	encoder = new ClickEncoder(ENCODER_DT, ENCODER_CLK, ENCODER_SW, 4);

	for (uint8_t i = 0; i < NUMBER_OF_KEYS; i++) {
		pinMode(keys[i].pin, INPUT_PULLUP);
	}

	Timer1.initialize(1000);
	Timer1.attachInterrupt(timerIsr);

	rotPosLast = -1;
}

void loop() {
	handleKeyPress();
	handleDialTurn();
	handleDialPress();
}
