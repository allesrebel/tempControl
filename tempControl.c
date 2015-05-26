#include <msp430.h> 
#include <stdio.h>
#include "mydriverlib.h"

/*
 * Port Definitions
 */
//	UART Port Definitions
#define	uartPSel1	P1SEL
#define uartPSel2	P1SEL2
#define uartRxPort	BIT1
#define uartTxPort	BIT2

//	UART is on UCA0!
#define	uartCtrl1	UCA0CTL1
#define uartBRate0	UCA0BR0
#define	uartBRate1	UCA0BR1
#define clkDiv9600	1666
#define uartModCtrl	UCA0MCTL
#define uartTxBuf	UCA0TXBUF

/*
 * Functions
 */
void setupClock();

/*
 * UART Functions
 */
void uartSetup();
void uartSend(char);
void uartSendStr(char*);

/*
 * UART Variables
 */
char test[30];
#define bufferSize 5
char buffer[bufferSize];	//Buffer for holding commands

#define UP 1
#define DOWN 0

//	Temperature Range Desired (in C)
int turnOff = 620;	//	79C = 1.553V or 482 with ADC10 3.3Vref
int turnOn = 530;	//	71C = 1.665V or 516 with ADC10 3.3Vref
int trend = DOWN;
int secondsLast = 0;
int offTime = 120;

/*
 * main.c
 */
int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	//set up phase!
	init_driverLib();
	uartSetup();

	_enable_interrupts();

	start_ADC();

	//	Initial Conditions Heat Off
	P1DIR |= BIT6;
	P1OUT &= ~BIT6;	//Turn off Heater

	while (1) {
		if(!(ADC10CTL1 & ADC10BUSY)){
			// Send the Information to Computer
			sprintf(test,"ADC Val: %d\n", ADC10MEM);
			uartSendStr(test);

			// Figure out how much time as passed
			int seconds = millis()/1000;
			int deltaSeconds = seconds - secondsLast;

			//	Edge Case - millis had to reset due to overflow
			if(deltaSeconds < 0)
				secondsLast = seconds;	// reset seconds

			/*
			 * Timed Logic
			 * On for 1 Min
			 * Off for 2 Mins
			 */
			if(trend == UP){
				//	Keep Heater On for a Minute
				if(deltaSeconds > 60){
					P1OUT &= ~BIT6;	// Turn Off the Heater
					trend = DOWN;
					secondsLast = seconds;
				}
			}
			else if(trend == DOWN){
				if(deltaSeconds > offTime){
					P1OUT |= BIT6;	// Turn On the Heater
					trend = UP;
					secondsLast = seconds;
				}
			}
			else
				P1OUT &= ~BIT6;		// Should never get here

			start_ADC();				// Take the Sample
		}
	}
}

void uartSendStr(char* data) {
	int i = 0;
	while (data[i] != '\0') {
		uartSend(data[i]);
		i++;
	}
}

void uartSend(char data) {
	while (!(IFG2 & UCA0TXIFG))
		; // Wait for TX buffer to be ready for new data
	uartTxBuf = data;
}

void uartSetup() {
	/* Configure hardware UART */
	uartPSel1 |= uartRxPort + uartTxPort;	// P1.1 = RXD, P1.2=TXD
	uartPSel2 |= uartRxPort + uartTxPort;	// P1.1 = RXD, P1.2=TXD
	uartCtrl1 |= UCSSEL_2;					// Use SMCLK as src clk
	uartBRate0 = (clkDiv9600 & 0xFF);// Set baud rate to 9600 with 16MHz clock
	uartBRate1 = (clkDiv9600 >> 8);	// Set baud rate to 9600 with 16MHz clock
	uartModCtrl = UCBRS0;		// Modulation UCBRSx = 1
	uartCtrl1 &= ~UCSWRST;	// Initialize USCI state machine
	IE2 |= UCA0RXIE;		// Enable USCI_A0 RX interrupt
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void UARTRx_ISR(void) {
	//process current char
	sprintf(test,"Settings Changed Off Time: %d\n", (int)UCA0RXBUF - '0');
	offTime = ((int)UCA0RXBUF - '0') * 60;
	uartSendStr(test);
}

//ADC10 interrupt Service Routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
	ADC10CTL0 &= ~ADC10IFG;  // clear interrupt flag
}

void setupClock() {
	WDTCTL = WDTPW + WDTHOLD;	// Stop WDT
	if (CALBC1_16MHZ == 0xFF)	// If calibration constant erased
			{
		while (1)
			;	// do not load, trap CPU!!
	}
	DCOCTL = 0;	// Select lowest DCOx and MODx settings
	BCSCTL1 = CALBC1_16MHZ;	// Set DCO
	DCOCTL = CALDCO_16MHZ;
}
