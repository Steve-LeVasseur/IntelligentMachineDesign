/*
 * IMD_Demo5-1_DigitalControl.cpp
 *
 * Created: 3/6/2023 4:00:16 PM
 * Author : 13305
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
//Define the codes for TWI actions
#define TWCR_START 0xA4|(1<<TWIE) //send start condition
#define TWCR_STOP 0x94|(1<<TWIE) //send stop condition
#define TWCR_RSTART 0xA4|(1<<TWIE) //send repeated start
#define TWCR_RACK 0xC4|(1<<TWIE) //receive byte and return ack to slave
#define TWCR_RNACK 0x84|(1<<TWIE) //receive byte and return nack to slave
#define TWCR_SEND 0x84|(1<<TWIE) //pokes the TWINT flag in TWCR and TWEN
//Function Prototypes
void InitializeDisplay();
void UpdateDisplay();
void TWI_Start();
void TWI_SendAddress(uint8_t);
void TWI_SendData(uint8_t);
void TWI_Stop();
void DummyLoop(uint16_t);
void UpdateTWIDisplayState();
//Global variables for display strings
volatile uint8_t FirstLineStr[21] =  "S2: XXXX            ";
volatile uint8_t SecondLineStr[21] = "M2: XXX  D2:X       ";
volatile uint8_t ThirdLineStr[21] =  "Button: OXX         ";
volatile uint8_t FourthLineStr[21] = "                    ";

//Global variables for TWI state and data
uint8_t display_state =0;
uint8_t TWI_data = 0;
uint8_t TWI_slave_address = 0;
uint8_t TWI_char_index = 0;

volatile unsigned char high_out1 = 0;
volatile unsigned char low_out1 = 0;
volatile unsigned char high_out2 = 0;
volatile unsigned char low_out2 = 0;
volatile uint32_t combined = 0;
volatile uint8_t display_counter = 0;
volatile uint8_t which_ad = 1;
volatile uint8_t button_temp = 0;
volatile uint8_t duty_temp = 0;

int main(void)
{
//**************************************************************************************
//Configure A/D
//**************************************************************************************
//Select Analog Port 0 and Internal Reference Voltage;
	ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(1<<MUX1)|(0<<MUX0);
	ADCSRA = (1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(0<<ADPS0); //Enable A/D, Enable Interrupt, Set A/D Prescalar
	DIDR0 = (1<<ADC0D); //Disable Input Buffers
	ADCSRA |= (1<<ADSC); //Start Conversion
	
	
	// Redo?
	//Configure I/O
	//Set SDA (PC4) and SCL (PC5) as outputs for TWI bus
	DDRC = (1<<PORTC5)|(1<<PORTC4);
	//Configure PD0 as output for debug output
	DDRD = (1<<PORTD0)|(1<<PORTD1)|(1<<PORTD2)|(1<<PORTD3)|(1<<PORTD5)|(1<<PORTD6);
	DDRB = (1<<PORTB2)|(1<<PORTB1)|(1<<PORTB0); //Set Pin PB0,PB1(OC1A),PB2 to output
	
	//******************************************************************************
	//Configure Timer 1 to generate an ISR every 100ms
	//******************************************************************************
	//No Pin Toggles, CTC Mode
	TCCR1A = (0<<COM1A1)|(1<<COM1A0)|(0<<COM1B1)|(0<<COM1B0)|(0<<WGM11)|(0<<WGM01);
	//CTC Mode, N=64
	TCCR1B = (0<<ICNC1)|(0<<ICES1)|(0<<WGM13)|(1<<WGM12)|(0<<CS12)|(1<<CS11)|(1<<CS10);
	//Enable Channel A Match Interrupt
	TIMSK1 = (0<<ICIE1)|(0<<OCIE1B)|(1<<OCIE1A)|(0<<TOIE1);
	//Setup output compare for channel A
	OCR1A = 5000; //OCR1A = 16Mhz/N*Deltat-1 = 16Mhz/64*.1-1
	
	//**************************************************************************************
	//Configure Timer 0
	//**************************************************************************************
	TCCR0A = (1<<COM0A1)|(0<<COM0A0)|(1<<COM0B1)|(1<<COM0B0)|(1<<WGM01)|(1<<WGM00); //Setup Fast PWM Mode for Channel A
	TCCR0B = (0<<WGM02)|(1<<CS02)|(0<<CS01)|(1<<CS00); //Define Clock Source Prescaler
	OCR0A = 64; //Initialized PWM Duty Cycle
	OCR0B = 64;
	
	//Configure TWI module (no interrupts)
	//Configure Bit Rate (TWBR)
	TWBR = 50;
	//Configure TWI Interface (TWCR)
	//TWINT: 0 - Interrupt Flag
	//TWEA: 0 - No acknowledge Bit
	//TWSTA: 0 - Start Condition Bit
	//TWSTO: 0 - Stop Condition
	//TWEN: 1 - Enable TWI Interface
	//TWIE: 0 - Disable Interrupt
	TWCR = (0<<TWINT)|(0<<TWEA)|(0<<TWSTO)|(0<<TWWC)|(1<<TWEN)|(1<<TWIE);
	//Configure TWI Status Register (TWSR)
	//TWS7-TWS3: 00000 - Don't change status
	//TWPS1-TWPS0: 10 - Prescaler Value = 64
	TWSR = (0<<TWS7)|(0<<TWS6)|(0<<TWS5)|(0<<TWS4)|(0<<TWS3)|(1<<TWPS1)|(0<<TWPS0);
	//Initialize Display
	display_state=17;//Set state machine to initialization section
	UpdateTWIDisplayState();//Run state machine
	sei();
	/* Replace with your application code */
	while (1)
	{
		//Toggle Pin
		PORTD ^= (1<<PIND0);
	}
}


//Interrupt Routine for Timer 1 Match THIS IS THE NEW ONE
ISR (TIMER1_COMPA_vect)
{
	ADCSRA |= (1<<ADSC); //Start Conversion
	display_counter++;
	button_temp = !((1<<PIND5)&PIND);
	if (display_counter>49){
	
	display_counter = 0;
	combined = low_out1;
	combined = (combined<<2);
	
	FirstLineStr[4] = (combined/1000)+48;
	combined = combined % 1000;
	FirstLineStr[5] = (combined/100)+48;
	combined = combined % 100;
	FirstLineStr[6] = (combined/10)+48;
	combined = combined % 10;
	FirstLineStr[7] = combined+48;
	
	combined = (low_out1<<2);
	combined = (combined*100)/1023;
	duty_temp = combined;
	
	SecondLineStr[4] = (combined/100)+48;
	combined = combined % 100;
	SecondLineStr[5] = (combined/10)+48;
	combined = combined % 10;
	SecondLineStr[6] = combined+48;
	// if button on or off, different results for this line
	if (button_temp)
	{
		
		ThirdLineStr[9] = 110; // n
		ThirdLineStr[10] = 16; // space	
	}
	else 
	{
		
		ThirdLineStr[9] = 102; // f
		ThirdLineStr[10] = 102; // f
	}
	if (duty_temp > 50)
	{
		SecondLineStr[12] = 49; // 1
	}
	else
	{
		SecondLineStr[12] = 48; // 0
	}
	
	// update LCD here
	display_state = 0;
	if(display_state==0)
	{
		UpdateTWIDisplayState();
	}
	
	}
}



//Interrupt Routine for A/D Completion
ISR (ADC_vect)
{
	// PORTB ^= 0x04;//Toggle Pin PB2
	low_out1 = ADCL;
	high_out1 = ADCH;
	low_out1 = (low_out1>>2);
	high_out1 = (high_out1<<6);
	low_out1 = (low_out1|high_out1);
	OCR0A = OCR0B = low_out1; //Load A/D High Register in OCR0A to set PWM Duty Cycle
	button_temp = !((1<<PIND5)&PIND);
	if (button_temp) // ccw
	{
		PORTB &= !(1<<PORTB0);
		PORTB |= (1<<PORTB1);
	}
	else // cw
	{
		PORTB &= !(1<<PORTB1);
		PORTB |= (1<<PORTB0);
	}
}



/************************************************************************/
/* Display Functions                                                    */
/************************************************************************/
void DummyLoop(uint16_t count)
{
	//Each index1 loop takes approx 100us
	for ( uint16_t index1=0; index1<=count; index1=index1+1 ) {
		//Each index2 loop takes .5us (200 loops = 100us)
		for ( uint16_t index2=0; index2<=200; index2=index2+1 ){
			//Do nothing
		}
	}
}
void UpdateTWIDisplayState()
{
	switch(display_state) {
		case 0: //Start of a new display update
		TWCR = TWCR_START;  //send start condition
		display_state++;
		break;
		case 1: //Send address
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 2: //Send Control Byte (Instruction: 0x80)
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 3: //Send Instruction Byte (Set DDRAM Address: 0x80)
		TWDR = 0x80; //Set DDRAM Address: 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 4: //Send Control Byte (DDRAM Data: 0x40)
		TWDR = 0x40; //DDRAM Data: 0x40
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 5: //Send First Line Character
		TWDR = FirstLineStr[TWI_char_index];
		if(TWI_char_index<19) //If not last character
		{
			TWI_char_index++; //Increment index
		}
		else //If last character
		{
			TWI_char_index = 0; //Reset index for next line
			display_state++; //Move to next line state
		}
		TWCR = TWCR_SEND; //Set TWINT to send data
		break;
		case 6: //Send Third Line Character
		TWDR = ThirdLineStr[TWI_char_index];
		if(TWI_char_index<19) //If not last character
		{
			TWI_char_index++; //Increment index
		}
		else //If last character
		{
			TWI_char_index = 0; //Reset index for next line
			display_state++; //Send stop signal
		}
		TWCR = TWCR_SEND; //Set TWINT to send data
		break;
		case 7: //Send repeated start to reset display
		TWCR = TWCR_RSTART;
		display_state++;
		break;
		case 8: //Send address
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 9: //Send Control Byte (Instruction: 0x80)
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 10: //Send Instruction Byte (Set DDRAM Address: 0x80)
		TWDR = 0xC0; //Set DDRAM Address: 0xC0
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 11: //Send Control byte for DDRAM data
		TWDR = 0x40;
		TWCR = TWCR_SEND; //Set TWINT to send data
		TWI_char_index =0;
		display_state++;
		break;
		case 12: //Send Second Line Characters
		TWDR = SecondLineStr[TWI_char_index];
		if(TWI_char_index<19) //If not last character
		{
			TWI_char_index++; //Increment index
		}
		else //If last character
		{
			TWI_char_index = 0; //Reset index for next line
			display_state++; //Send stop signal
		}
		TWCR = TWCR_SEND; //Set TWINT to send data
		break;
		case 13: //Send Fourth Line Characters
		TWDR = FourthLineStr[TWI_char_index];
		if(TWI_char_index<19) //If not last character
		{
			TWI_char_index++; //Increment index
		}
		else //If last character
		{
			TWI_char_index = 0; //Reset index for next line
			display_state++; //Send stop signal
		}
		TWCR = TWCR_SEND; //Set TWINT to send data
		break;
		case 14: //Create Stop Condition
		TWCR = TWCR_STOP;//finish transaction
		display_state =0;
		break;
		/************************************************************************/
		/* Initialization States
		*/
		/************************************************************************/
		case 17: //Initialize Step One
		DummyLoop(400);//Wait 40ms for powerup
		TWCR = TWCR_START;
		display_state++;
		break;
		case 18:
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 19: //Send control byte for instruction
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 20: //Set Function Mode
		TWDR = 0x38;
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 21: //Send Stop and Wait
		TWCR = TWCR_STOP;//finish transaction
		DummyLoop(1);//Delay 100us
		TWCR = TWCR_START;//Start next transaction
		display_state++;
		break;
		case 22: //Send Slave Address
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 23: //Send control byte for instruction
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 24: //Turn on Display, Cursor, and Blink
		TWDR = 0x0C; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 25: //Send Stop and Wait
		TWCR = TWCR_STOP;//finish transaction
		DummyLoop(1);//Delay 100us
		TWCR = TWCR_START;//Start next transaction
		display_state++;
		break;
		case 26: //Send Slave Address
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 27: //Send control byte for instruction
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 28: //Clear Display
		TWDR = 0x01; //Instruction 0x01
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 29: //Send Stop and Wait
		TWCR = TWCR_STOP;//finish transaction
		DummyLoop(100);//Delay 10ms
		TWCR = TWCR_START;//Start next transaction
		display_state++;
		break;
		case 30: //Send Slave Address
		TWDR = 0x78; //Set TWI slave address
		TWCR = TWCR_SEND; //Send address
		display_state++;
		break;
		case 31: //Send control byte for instruction
		TWDR = 0x80; //Instruction 0x80
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 32: //Clear Display
		TWDR = 0x01; //Instruction 0x01
		TWCR = TWCR_SEND; //Set TWINT to send data
		display_state++;
		break;
		case 33: //Send Stop and Wait
		TWCR = TWCR_STOP;//finish transaction
		DummyLoop(1);//Delay 100us
		display_state=0;
		break;
		default:
		display_state = 0;
	}
}
/************************************************************************/
/* Define TWI Functions                                                 */
/************************************************************************/
ISR(TWI_vect)
{
	PORTD ^= (1<<PIND2);
	//Read status register and mask out prescaler bits
	uint8_t status = TWSR & 0xF8;
	//Switch based on status of TWI interface
	switch(status)
	{
		case 0x08: //Start Condition Transmitted
		UpdateTWIDisplayState();
		break;
		case 0x10: //Repeat Start Condition Transmitted
		UpdateTWIDisplayState();
		break;
		case 0x18: //SLA+W transmitted and ACK received
		UpdateTWIDisplayState();
		break;
		case 0x20: //SLA+W transmitted and ACK NOT received
		//This is an error, do something application specific
		break;
		case 0x28: //Data byte transmitted and ACK received
		UpdateTWIDisplayState();
		break;
		case 0x30: //Data byte transmitted and ACK NOT received
		//This is an error, do something application specific
		break;
	}
	PORTD ^= (1<<PIND2);
}

