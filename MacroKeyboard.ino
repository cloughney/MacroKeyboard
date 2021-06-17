#include <Arduino.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <HID-Project.h>

#define NUMBER_OF_KEYS			8
#define MAX_COMBINATION_KEYS	4

#define DEBOUNCING_MS			20
#define FIRST_REPEAT_CODE_MS	500
#define REPEAT_CODE_MS			150

#define ENCODER_CLK				A0
#define ENCODER_DT				A1
#define ENCODER_SW				A2

enum TKeyState {INACTIVE, DEBOUNCING, ACTIVE, HOLDING};

struct TKey {
	uint8_t pin;
	enum TKeyState state;
	uint32_t stateStartMs;
	uint16_t keys[MAX_COMBINATION_KEYS];
};

TKey keys[NUMBER_OF_KEYS] = {
	{ .pin =  6, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F13 } },
	{ .pin =  7, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F14 } },
	{ .pin =  8, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F15 } },
	{ .pin =  9, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F16 } },
	{ .pin = 15, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F17 } },
	{ .pin = 14, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F18 } },
	{ .pin =  5, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F19 } },
	{ .pin = 10, .state = INACTIVE, .stateStartMs = 0, .keys = { KEY_F20 } }
};

ClickEncoder *encoder;
int16_t rotPosLast, rotPosCurrent;

void timerIsr() {
	encoder->service();
}

bool stateExceedsThreshold(uint8_t keyIndex, uint16_t threshold) {
	return millis() - keys[keyIndex].stateStartMs > threshold;
}

void resetState(uint8_t keyIndex) {
	TKey* key = &keys[keyIndex];

	key->state = INACTIVE;
	key->stateStartMs = millis();
}

uint8_t sendKeyPress(uint8_t keyIndex) {
	TKey *key = &keys[keyIndex];

	for (uint8_t i = 0; i < MAX_COMBINATION_KEYS; i++) {
		if (key->keys[i]) {
			Keyboard.press((KeyboardKeycode)key->keys[i]);
		} else {
			break;
		}
	}

	Keyboard.releaseAll();
}

void handleKeyPress() {
	for (uint8_t i = 0; i < NUMBER_OF_KEYS; i++) {
		bool pressed = digitalRead(keys[i].pin) == LOW;

		switch (keys[i].state) {

			case INACTIVE:
				if (pressed) {
					keys[i].state = DEBOUNCING;
					keys[i].stateStartMs = millis();
				}
				break;

			case DEBOUNCING:
				if (!pressed) {
					resetState(i);
				} else if (stateExceedsThreshold(i, DEBOUNCING_MS)) {
					keys[i].state = ACTIVE;
					sendKeyPress(i);
				}
				break;
			
			case ACTIVE:
				if (!pressed) {
					resetState(i);
				} else if (stateExceedsThreshold(i, FIRST_REPEAT_CODE_MS)) {
					keys[i].state = HOLDING;
					keys[i].stateStartMs = millis();
					sendKeyPress(i);
				}
				break;

			case HOLDING:
				if (!pressed) {
					resetState(i);
				} else if (stateExceedsThreshold(i, REPEAT_CODE_MS)) {
					keys[i].stateStartMs = millis();
					sendKeyPress(i);
				}
				break;
		}
	}
}

void handleDialTurn() {
	rotPosCurrent += encoder->getValue();

	if (rotPosCurrent != rotPosLast) {
		uint16_t diff = abs(rotPosCurrent - rotPosLast);
		
		KeyboardKeycode key = (rotPosCurrent > rotPosLast) ? KEY_F23 : KEY_F24;
		for (uint8_t i = 0; i < diff; i++) {
			Keyboard.press(key);
			Keyboard.release(key);
		}

		rotPosLast = rotPosCurrent;
	}
}

void handleDialPress() {
	ClickEncoder::Button b = encoder->getButton();
	if (b != ClickEncoder::Open) {
		switch (b) {
			case ClickEncoder::Clicked:
				Keyboard.press(KEY_F21);
				Keyboard.release(KEY_F21);
				break;
			case ClickEncoder::DoubleClicked:
				Keyboard.press(KEY_F22);
				Keyboard.release(KEY_F22);
				break;
		}
	}
}

void setup() {
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
