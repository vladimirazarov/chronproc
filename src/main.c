/*
 * Author: Vladimir Azarov
 * Filename: main.c
 * Description: This program is designed to run on a microcontroller with specific hardware features.
 * It implements a digital clock with an alarm system, allowing the user to set the time, alarm time, melody, and light effects.
 * It handles user input through UART and controls LEDs and a speaker for alarm notifications.
 */

#include "MK60D10.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>

// Define constants for LED, buttons, and speaker hardware connections
#define LED_D9  0x20      // Port B, bit 5
#define LED_D10 0x10      // Port B, bit 4
#define LED_D11 0x8       // Port B, bit 3
#define LED_D12 0x4       // Port B, bit 2

#define BTN_SW2 0x400     // Port E, bit 10
#define BTN_SW3 0x1000    // Port E, bit 12
#define BTN_SW4 0x8000000 // Port E, bit 27
#define BTN_SW5 0x4000000 // Port E, bit 26
#define BTN_SW6 0x800     // Port E, bit 11

#define SPK 0x10          // Speaker is on PTA4

#define TOTAL_NOTES 10 // Total number of notes in the melody
#define TOTAL_LIGHT_STATES 20 // Total number of light states

// Enum for tracking the state of the user interface
enum InterfaceState {
	IDLE, READING_INPUT, PROCESSING_INPUT
};

enum InterfaceState interfaceState = IDLE; // Current state of the interface

// Global variables for program functionality
char inputBuffer[100]; // Buffer to store user input from UART
int inputIndex = 0; // Index for the input buffer

// Variables for alarm, melody, and light control
int selectedMelodyID = 1;
int selectedLightEffectID = 1;
bool alarmEnabled = false;
int alarmRepeatCount = 5;
int alarmIntervalSeconds = 5;

// Variables for time tracking
time_t globalAlarmTime = 0; // Time for the alarm to go off
bool isPlayingMelody = false; // Flag to check if melody is playing
bool isShowingLights = false; // Flag to check if lights are showing
int melodyIndex = 0; // Index for the current note in the melody
int lightIndex = 0; // Index for the current light state

// Function prototypes for various utility, initialization, and control functions
void UARTSendStr(char* str);
bool UARTReceiveStr(char* buffer, int bufferSize);
void handleAlarmRepeats();
void chooseMelody();
void chooseLightEffect();
void toggleAlarm(int enable);
void displayAlarmStatus();
void setAlarmRepeat();
void Delay(long long bound);
void MCUInit();
void MakeSound(uint32_t duration_ms);
void UARTInit();
void PortsInit();
void RTCInit();
void setClock();
void setAlarm();
void playMelody(int melodyID);
void lightSignal(int signalID);
bool getUserTimeInput(int *year, int *month, int *day, int *hour, int *min,
		int *sec);
void RTC_IRQHandler();
void startMelody(int melodyID);
void startLightEffect(int lightEffectID);
void playNextNote();
void updateLights();
void SendCh(char ch);
void checkUserInput();
void processUserInput(char* input);
void displayMenu();
int main(void);

/**
 * Initiates the playing of a selected melody.
 *
 * @param melodyID The ID of the melody to be played.
 */
void startMelody(int melodyID) {
	selectedMelodyID = melodyID;
	melodyIndex = 0;
	isPlayingMelody = true;
}

/**
 * Initiates a selected light effect.
 *
 * @param lightEffectID The ID of the light effect to be initiated.
 */
void startLightEffect(int lightEffectID) {
	selectedLightEffectID = lightEffectID;
	lightIndex = 0;
	isShowingLights = true;
}

/**
 * Plays the next note in the selected melody.
 */
void playNextNote() {
	if (melodyIndex < TOTAL_NOTES) {
		switch (selectedMelodyID) {
		case 1:
			MakeSound(50000 + melodyIndex * 5000);
			Delay(50000);
			break;
		case 2:

			MakeSound(100000 + melodyIndex * 10000);
			Delay(10000);
			MakeSound(100000 + melodyIndex * 10000);
			Delay(10000);
			MakeSound(100000 + melodyIndex * 10000);
			break;
		case 3:

			MakeSound(10000 + melodyIndex * 5000);
			Delay(2000);
			MakeSound(100000 + melodyIndex * 5000);
			Delay(10000);
			MakeSound(10000 + melodyIndex * 5000);
			Delay(5000);
			MakeSound(100000 + melodyIndex * 5000);
			Delay(1000);
			break;
		}
		melodyIndex++;
	} else {
		isPlayingMelody = false;
	}
}
/**
 * Updates the lights based on the selected effect.
 */
void updateLights() {
	if (lightIndex < TOTAL_LIGHT_STATES) {
		switch (selectedLightEffectID) {
		case 1:
			// All LEDs on/off toggle pattern
			if (lightIndex % 2 == 0) {
				PTB->PDOR &= ~0x3C; // Turn all LEDs on
			} else {
				PTB->PDOR |= 0x3C;  // Turn all LEDs off
			}
			Delay(200000);
			break;
		case 2:
			// Sequential lighting pattern
			PTB->PDOR = ~(1 << lightIndex); // Turn on one LED at a time
			Delay(200000);
			break;
		case 3:
			// Rotating light pattern
			PTB->PDOR = ~(1 << (lightIndex % 4 + 2)); // Rotate through LEDs
			Delay(200000);
			break;
		}
		lightIndex++;
	} else {
		isShowingLights = false;
	}
}
/**
 * Sends a character via UART.
 *
 * @param ch Character to be sent.
 */
void SendCh(char ch) {
	while (!(UART5->S1 & UART_S1_TDRE_MASK) && !(UART5->S1 & UART_S1_TC_MASK))
		;
	UART5->D = ch;
}
/**
 * Sends a string via UART.
 *
 * @param s String to be sent.
 */
void UARTSendStr(char *s) {
	int i = 0;
	while (s[i] != 0) {
		SendCh(s[i++]);
		if (s[i - 1] == '\n') {
			SendCh('\r');
		}
	}
}

/**
 * Reads a string from UART until a newline character is received or the buffer is full.
 *
 * @param buffer The buffer where the received string will be stored.
 * @param bufferSize The size of the buffer.
 * @return Returns true if only a newline or carriage return is received, false otherwise.
 */
bool UARTReceiveStr(char* buffer, int bufferSize) {
	int i = 0;
	char c;

	// Receive characters until newline or buffer is full
	do {
		while (!(UART5_S1 & UART_S1_RDRF_MASK))
			; // Wait for receive buffer to be full
		c = UART5->D; // Read the received character
		if (c == '\n' || c == '\r') {
			if (i == 0) { // if it's the first character received
				buffer[0] = '\0'; // Null-terminate the string
				return true; // Return true indicating only newline/carriage return was received
			}
			break;
		}
		if (c == '\b' || c == '\177') { // Backspace character
			if (i > 0) {
				i--; // Remove the previous character
			}
		} else if (i < bufferSize - 1) {
			buffer[i++] = c;
		}
	} while (i < bufferSize - 1);

	buffer[i] = '\0'; // Null-terminate the string
	return false; // Normal input, return false
}
/**
 * Handles the repetition of the alarm based on the set configuration.
 */
void handleAlarmRepeats() {
	static int currentRepeatCount = 1;
	if (alarmEnabled) {
		if (currentRepeatCount < alarmRepeatCount + 1) {
			// Calculate the next alarm time
			time_t nextAlarmTime = globalAlarmTime
					+ (currentRepeatCount * alarmIntervalSeconds);

			// Format the time of the next alarm
			char nextTimeStr[50];
			struct tm *nextAlarmTimeInfo = localtime(&nextAlarmTime);
			strftime(nextTimeStr, sizeof(nextTimeStr), "%Y-%m-%d %H:%M:%S",
					nextAlarmTimeInfo);

			// Print attempt ID and next alarm time with formatted text
			char buffer[500];
			snprintf(buffer, sizeof(buffer),
					"\033[1;3;31m\nPokus o buzeni %d\033[0m, \033[1;3;32mDalsi Alarm: %s\033[0m\n",
					currentRepeatCount, nextTimeStr);
			UARTSendStr(buffer);

			// Set the RTC alarm to the next alarm time
			RTC_TAR = (uint32_t) nextAlarmTime;

			// Increase the repeat count
			currentRepeatCount++;

			// Initialize the alarm (melody and lights)
			startMelody(selectedMelodyID);
			startLightEffect(selectedLightEffectID);
		} else {
			// If all repeats are done
			currentRepeatCount = 1; // Reset the repeat count
			RTC_TAR = 0;            // Reset o the RTC alarm register
		}
	} else {
		currentRepeatCount = 1; // Reset the repeat count
		RTC_TAR = 0;            // Reset the RTC alarm register
	}
}
/**
 * RTC interrupt handler to manage alarm repeats and display.
 */
void RTC_IRQHandler() {
	// Check if the alarm interrupt flag is set
	if (RTC_SR & RTC_SR_TAF_MASK) {
		handleAlarmRepeats();
		if (alarmEnabled) {
			while (isPlayingMelody && isShowingLights) {
				playNextNote();
				updateLights();
			}
			PTB->PDOR |= 0x3C;  // Turn all LEDs off
		}
		displayMenu();
	}
}

/**
 * Allows the user to choose a melody for the alarm.
 */
void chooseMelody() {
	char buffer[100];

	UARTSendStr("\033[1;37mVyberte melodii (1-3): \033[0m");
	UARTReceiveStr(buffer, sizeof(buffer));
	int melodyChoice = atoi(buffer);

	if (melodyChoice >= 1 && melodyChoice <= 3) {
		selectedMelodyID = melodyChoice;
		UARTSendStr("\033[1;32m\nMelodie efekt byl vybrana.\n\033[0m");
	} else {
		UARTSendStr(
				"\033[1;31m\nNeplatná volba, zadejte číslo mezi 1 a 3.\n\033[0m");
	}
}

/**
 * Allows the user to choose a light effect for the alarm.
 */
void chooseLightEffect() {
	char buffer[100];

	UARTSendStr("\033[1;37mVyberte světelný efekt (1-3): \033[0m");
	UARTReceiveStr(buffer, sizeof(buffer));
	int lightEffectChoice = atoi(buffer);

	if (lightEffectChoice >= 1 && lightEffectChoice <= 3) {
		selectedLightEffectID = lightEffectChoice;
		UARTSendStr("\033[1;32m\nSvětelný efekt byl vybrán.\n\033[0m");
	} else {
		UARTSendStr(
				"\033[1;31m\nNeplatná volba, zadejte číslo mezi 1 a 3.\n\033[0m");
	}
}
/**
 * Toggles the alarm state between enabled and disabled based on the input.
 *
 * @param enable If 1, the alarm is enabled; if 0, the alarm is disabled.
 */
void toggleAlarm(int enable) {
	if (enable == 1) {
		alarmEnabled = true;
		UARTSendStr("\033[1;32mAlarm byl zapnut.\n\033[0m");
	} else if (enable == 0) {
		alarmEnabled = false;
		UARTSendStr("\033[1;32mAlarm byl vypnut.\n\033[0m");
	} else {
		UARTSendStr(
				"\033[1;31m\nNeplatná volba, zadejte 1 pro zapnutí nebo 0 pro vypnutí alarmu.\n\033[0m");
	}
}

/**
 * Displays the current status of the alarm including time, melody, and light effect settings.
 */
void displayAlarmStatus() {
	char buffer[500];
	char alarmTimeStr[50];
	char currentTimeStr[50];
	time_t currentTime;

	// Get the current time from RTC
	currentTime = RTC_TSR;

	// Convert the current time to a readable format
	struct tm *currentTimeInfo = localtime(&currentTime);
	strftime(currentTimeStr, sizeof(currentTimeStr), "%Y-%m-%d %H:%M:%S",
			currentTimeInfo);

	// Convert the global alarm time to a readable format
	struct tm *alarmTimeInfo = localtime(&globalAlarmTime);
	strftime(alarmTimeStr, sizeof(alarmTimeStr), "%Y-%m-%d %H:%M:%S",
			alarmTimeInfo);

	// Create the status message with both the current time, alarm time, repetition count, and interval
	UARTSendStr("\033[30;47m\nStav alarmu\033[0m\n");
	snprintf(buffer, sizeof(buffer),
			"\033[1;32m Alarm je %s\n\033[0m"  // Bold green for "Alarm je"
					"\033[0;36m Čas alarmu: %s\n\033[0m"// Cyan for "Čas alarmu"
					"\033[0;33m Aktuální čas: %s\n\033[0m"// Yellow for "Aktuální čas"
					"\033[0;35m Vybraná melodie: %d\n\033[0m"// Magenta for "Vybraná melodie"
					"\033[0;35m Vybraný světelný efekt: %d\n\033[0m"// Magenta for "Vybraný světelný efekt"
					"\033[0;33m Počet opakování alarmu: %d\n\033[0m"// Yellow for "Počet opakování alarmu"
					"\033[0;33m Interval opakování (v sekundách): %d\n\033[0m",// Yellow for "Interval opakování"
			alarmEnabled ?
					"\033[1;32mzapnut\033[0m" : "\033[1;31mvypnut\033[0m", // Green for "zapnut", Red for "vypnut"
			alarmTimeStr, currentTimeStr, selectedMelodyID,
			selectedLightEffectID,
			alarmRepeatCount, alarmIntervalSeconds);

	UARTSendStr(buffer);
}
/**
 * Allows the user to set the number of alarm repetitions and the interval between them.
 */
void setAlarmRepeat() {
	char buffer[100];

	// Get the repeat count from the user
	UARTSendStr(
			"\033[1;37m\nZadejte počet opakování budíku (0 pro žádné opakování): \033[0m");
	UARTReceiveStr(buffer, sizeof(buffer));
	int repeatCount = atoi(buffer);

	if (repeatCount >= 0) {
		alarmRepeatCount = repeatCount;

		// Get the interval between repetitions
		UARTSendStr(
				"\033[1;37m\nZadejte interval mezi opakováními v sekundách: \033[0m");
		UARTReceiveStr(buffer, sizeof(buffer));
		int intervalSeconds = atoi(buffer);

		if (intervalSeconds > 0) {
			alarmIntervalSeconds = intervalSeconds;
			UARTSendStr(
					"\033[1;32m\nNastavení opakování budíku bylo aktualizováno.\n\033[0m");
		} else {
			UARTSendStr(
					"\033[1;31m\nNeplatný interval, musí být větší než 0.\n\033[0m");
		}
	} else {
		UARTSendStr(
				"\033[1;31m\nNeplatný počet opakování, musí být nezáporné číslo.\n\033[0m");
	}
}
/**
 * Implements a simple delay.
 *
 * @param bound The duration of the delay.
 */
void Delay(long long bound) {

	long long i;
	for (i = 0; i < bound; i++)
		;
}

/**
 * Initializes the Microcontroller Unit (MCU) settings.
 */
void MCUInit() {
	MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01));
	SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
	WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
}
/**
 * Generates a sound from the speaker for a specified duration.
 *
 * @param duration_ms Duration of the sound in milliseconds.
 */
void MakeSound(uint32_t duration_ms) {
	// Activate the speaker
	PTA->PSOR = SPK;

	// Add a delay to keep the speaker active for the specified duration
	Delay(duration_ms);

	// Deactivate the speaker
	PTA->PCOR = SPK;
}
/**
 * Initializes the UART5 peripheral with specific settings for communication.
 */
void UARTInit() {
	UART5->C2 &= ~(UART_C2_RE_MASK | UART_C2_TE_MASK);
	UART5->BDH = 0;
	UART5->BDL = 0x1A;
	UART5->C4 = 0x0F;
	UART5->C1 = 0;
	UART5->C3 = 0;
	UART5->MA1 = 0;
	UART5->MA2 = 0;
	UART5->S2 |= 0xC0;
	UART5->C2  |= ( UART_C2_TE_MASK | UART_C2_RE_MASK );
}
/**
 * Initializes the ports used by LEDs, buttons, and the speaker.
 */
void PortsInit() {
	// Initialize all port clocks
	SIM->SCGC5 = SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK
			| SIM_SCGC5_PORTA_MASK;
	SIM->SCGC1 = SIM_SCGC1_UART5_MASK;
	SIM->SCGC6 = SIM_SCGC6_RTC_MASK;

	// Configure LED pins as GPIO outputs
	PORTB->PCR[5] = PORT_PCR_MUX(0x01); // For LED D9
	PORTB->PCR[4] = PORT_PCR_MUX(0x01); // For LED D10
	PORTB->PCR[3] = PORT_PCR_MUX(0x01); // For LED D11
	PORTB->PCR[2] = PORT_PCR_MUX(0x01); // For LED D12

	// Configure button pins as GPIO inputs
	PORTE->PCR[10] = PORT_PCR_MUX(0x01); // For button SW2
	PORTE->PCR[12] = PORT_PCR_MUX(0x01); // For button SW3
	PORTE->PCR[27] = PORT_PCR_MUX(0x01); // For button SW4
	PORTE->PCR[26] = PORT_PCR_MUX(0x01); // For button SW5
	PORTE->PCR[11] = PORT_PCR_MUX(0x01); // For button SW6

	// Configure speaker pin
	PORTA->PCR[4] = PORT_PCR_MUX(0x01);

	// Set LED pins to output and turn them off
	PTB->PDDR |= GPIO_PDDR_PDD(0x3C);
	PTB->PDOR |= GPIO_PDOR_PDO(0x3C);

	PORTE->PCR[8] = PORT_PCR_MUX(0x03); // For UART transmitter
	PORTE->PCR[9] = PORT_PCR_MUX(0x03); // For UART receiver

	// Set speaker pin to output
	PTA->PDDR |= GPIO_PDDR_PDD(SPK);
	PTA->PDOR &= ~SPK;
}

/**
 * Initializes the Real-Time Clock (RTC) peripheral.
 */
void RTCInit() {
	// Reset RTC registers
	RTC_CR |= RTC_CR_SWR_MASK;
	RTC_CR &= ~RTC_CR_SWR_MASK;

	// Reset CIR and TCR
	RTC_TCR = 0;

	// Enable 32.768 kHz crystal oscillator
	RTC_CR |= RTC_CR_OSCE_MASK;

	// Wait for the oscillator to stabilize
	Delay(0x600000);

	// Disable the RTC module to configure it
	RTC_SR &= ~RTC_SR_TCE_MASK;

	// Set the time counter to a known value
	RTC_TSR = 0x00000000;

	// Set the alarm time
	RTC_TAR = 0xFFFFFFFF;

	// Time Alarm Interrupt Enable
	RTC_IER |= RTC_IER_TAIE_MASK;

	// Clear any pending RTC interrupts and enable the RTC interrupt
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_EnableIRQ(RTC_IRQn);

	// Enable the RTC again
	RTC_SR |= RTC_SR_TCE_MASK;
}

/**
 * Gets and validates user input for time settings.
 *
 * @param year Pointer to store year.
 * @param month Pointer to store month.
 * @param day Pointer to store day.
 * @param hour Pointer to store hour.
 * @param min Pointer to store minute.
 * @param sec Pointer to store second.
 * @return True if input is valid, False otherwise.
 */
bool getUserTimeInput(int *year, int *month, int *day, int *hour, int *min,
		int *sec) {
	char buffer[100];
	UARTSendStr(
			"\033[1;37m\nZadejte datum a čas (YYYY-MM-DD HH:MM:SS): \033[0m");
	UARTReceiveStr(buffer, sizeof(buffer));

	// Parse the input string and check for successful conversion
	if (sscanf(buffer, "%d-%d-%d %d:%d:%d", year, month, day, hour, min, sec)
			== 6) {
		// Validate the input values (basic validation)
		if (*year > 1900 && *month >= 1 && *month <= 12 && *day >= 1
				&& *day <= 31 && *hour >= 0 && *hour < 24 && *min >= 0
				&& *min < 60 && *sec >= 0 && *sec < 60) {
			return true;
		} else {
			UARTSendStr(
					"\033[1;31m\nNeplatný vstup, zadejte datum a čas v správném formátu.\n\033[0m");
			return false;
		}
	} else {
		UARTSendStr(
				"\033[1;31m\nChybný formát vstupu, zkuste to znovu.\n\033[0m");
		return false;
	}
}

/**
 * Sets the current time in the RTC.
 */
void setClock() {
	int year, month, day, hour, minute, second;
	bool timeSet = false; // Flag to check if time was set

	// Get the current time from the user.
	if (getUserTimeInput(&year, &month, &day, &hour, &minute, &second)) {
		// Convert the user input time to a format suitable for the RTC
		struct tm timeStruct;
		timeStruct.tm_year = year - 1900; // tm_year is the number of years since 1900
		timeStruct.tm_mon = month - 1;    // tm_mon is 0-based (0 for January)
		timeStruct.tm_mday = day;
		timeStruct.tm_hour = hour;
		timeStruct.tm_min = minute;
		timeStruct.tm_sec = second;

		// Convert to time_t (UNIX timestamp)
		time_t time = mktime(&timeStruct);

		// Disable the RTC before setting the time
		RTC_SR &= ~RTC_SR_TCE_MASK;

		// Set the time in the RTC
		RTC_TSR = (uint32_t) time;

		// Re-enable the RTC
		RTC_SR |= RTC_SR_TCE_MASK;
		timeSet = true; // Set the flag as true since time was set
	}

	if (timeSet) {
		UARTSendStr("\033[1;32m\nČas byl nastaven.\n\033[0m");
	} else {
		UARTSendStr("\033[1;31m\nČas nebyl nastaven.\n\033[0m");
	}
}
/**
 * Sets the alarm time.
 */
void setAlarm() {
	int year, month, day, hour, minute, second;
	bool alarmSet = false; // Flag to check if alarm was set

	// Get the alarm time from the user
	if (getUserTimeInput(&year, &month, &day, &hour, &minute, &second)) {
		struct tm alarmTime;
		alarmTime.tm_year = year - 1900;
		alarmTime.tm_mon = month - 1;
		alarmTime.tm_mday = day;
		alarmTime.tm_hour = hour;
		alarmTime.tm_min = minute;
		alarmTime.tm_sec = second;
		alarmTime.tm_isdst = -1;

		// Store the alarm time in global variable
		globalAlarmTime = mktime(&alarmTime);

		// Set the alarm in RTC
		RTC_SR &= ~RTC_SR_TCE_MASK;
		RTC_TAR = (uint32_t) globalAlarmTime;
		RTC_SR |= RTC_SR_TCE_MASK;

		alarmSet = true; // Set the flag as true since alarm was set
	}

	if (alarmSet) {
		UARTSendStr("\033[1;32m\nAlarm byl nastaven.\n\033[0m");
	} else {
		UARTSendStr("\033[1;31m\nAlarm nebyl nastaven.\n\033[0m");
	}
}
/**
 * Handles the alarm based on the set parameters.
 */
void handleAlarm() {
	// Initialize the melody and light effect
	startMelody(selectedMelodyID); // Function to initialize the melody
	startLightEffect(selectedLightEffectID); // Function to initialize the light effect
	isPlayingMelody = true;
	isShowingLights = true;
}
/**
 * Checks and processes user input received through UART.
 */
void checkUserInput() {
	char receivedChar;

	switch (interfaceState) {
	case IDLE:
		// Check if there's data available
		if (UART5_S1 & UART_S1_RDRF_MASK) {
			interfaceState = READING_INPUT;
			inputIndex = 0;
		}
		break;

	case READING_INPUT:
		// Ensure there's data available before reading from UART
		if (UART5_S1 & UART_S1_RDRF_MASK) {
			receivedChar = UART5_D;
			if (receivedChar != '\n' && receivedChar != '\r'
					&& inputIndex < sizeof(inputBuffer) - 1) {
				inputBuffer[inputIndex++] = receivedChar;
			} else {
				inputBuffer[inputIndex] = '\0'; // Null-terminate the string
				interfaceState = PROCESSING_INPUT;
			}
		}
		break;

	case PROCESSING_INPUT:
		// Process the received input
		processUserInput(inputBuffer);
		interfaceState = IDLE;
		break;
	}
}
/**
 * Processes user input and executes corresponding actions.
 *
 * @param input User input string.
 */
void processUserInput(char* input) {
	int choice = atoi(input); // Convert input to integer

	switch (choice) {
	case 1:
		setClock();
		displayMenu();
		break;
	case 2:
		setAlarm();
		displayMenu();
		break;
	case 3: {
		char buffer[100];  // Buffer to hold the input string
		UARTSendStr("\033[32m\n1 - zapnout\033[0m\n");
		UARTSendStr("\033[31m0 - vypnout\033[0m\n");
		UARTReceiveStr(buffer, sizeof(buffer));
		// Extracting the enable/disable value from the input
		int enable;
		if (sscanf(buffer, "%d", &enable) == 1) {
			// If successfully extracted, toggle the alarm
			toggleAlarm(enable);
		} else {
			UARTSendStr(
					"\033[1;31m\nChybný formát vstupu pro zapnutí/vypnutí alarmu.\n\033[0m");
		}
		displayMenu();
		break;
	}

	case 4:
		chooseMelody();
		displayMenu();
		break;
	case 5:
		chooseLightEffect();
		displayMenu();
		break;
	case 6:
		setAlarmRepeat();
		displayMenu();
		break;
	case 7:
		displayAlarmStatus();
		displayMenu();
		break;
	default:
		displayMenu();
		break;
	}
}
/**
 * Displays the main menu to the user through UART.
 */
void displayMenu() {

	UARTSendStr("\033[30;47m\nDigitální Hodiny s Budíkem\033[0m\n");

	// Menu Choices in Bold with Different Colors
	UARTSendStr(
			"\033[1;31m1. Nastavit Čas\033[0m - Nastavte aktuální čas hodin.\n");
	UARTSendStr(
			"\033[1;32m2. Nastavit Alarm\033[0m - Nastavte čas, kdy má alarm zazvonit.\n");
	UARTSendStr(
			"\033[1;33m3. Zapnout/Vypnout Alarm\033[0m - Zapněte nebo vypněte alarm.\n");
	UARTSendStr(
			"\033[1;34m4. Vybrat Melodii\033[0m - Vyberte melodii pro alarm.\n");
	UARTSendStr(
			"\033[1;35m5. Vybrat Světelný Efekt\033[0m - Vyberte světelný efekt pro alarm.\n");
	UARTSendStr(
			"\033[1;36m6. Nastavit Opakování Alarmu\033[0m - Nastavte opakování a interval alarmu.\n");
	UARTSendStr(
			"\033[1;37m7. Zobrazit Informace o Budíku\033[0m - Zobrazte aktuální nastavení alarmu.\n");

	UARTSendStr("\033[1;5;37mZadejte volbu: \033[0m");
}
/**
 * Main function
 */
int main(void) {
	MCUInit();
	PortsInit();
	UARTInit();
	RTCInit();

	UARTSendStr("\033[1;32mInicializace byla dokončena.\n\033[0m");

	while (1) {
		checkUserInput();
	}
	return 0;
}
