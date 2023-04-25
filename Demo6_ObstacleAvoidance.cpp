/*
 * IMD_Demo6.cpp
 *
 * Created: 3/27/2023 4:20:34 PM
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
volatile uint8_t FirstLineStr[21] =  "S1: XXXX            ";
volatile uint8_t SecondLineStr[21] = "S2: XXXX            ";
volatile uint8_t ThirdLineStr[21] =  "L: X B: X           ";
volatile uint8_t FourthLineStr[21] = "R: X                ";

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
volatile uint8_t b_count = 0;
volatile uint8_t e_count = 0;
//volatile uint8_t thresh = 0b11111010;
volatile uint8_t thresh = 75;
volatile uint8_t thresh_back = 110;
volatile uint8_t lastTurn = 0;
volatile uint16_t sensor_outright = 0;
volatile uint16_t sensor_outleft = 0;
volatile uint16_t sensor_outfront = 0;
volatile uint16_t sensor_outback = 0;
volatile uint8_t left0right1 = 0;
volatile uint8_t osc_count = 0;


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
	DDRD = (1<<PORTD0)|(1<<PORTD1)|(0<<PORTD2)|(0<<PORTD3)|(1<<PORTD5)|(1<<PORTD6);
	DDRB = (1<<PORTB3)|(1<<PORTB2)|(1<<PORTB1)|(1<<PORTB0); //Set Pin PB0,PB1(OC1A),PB2 to output

	
	//******************************************************************************
	//Configure Timer 1 to generate an ISR every 100ms
	//******************************************************************************
	//No Pin Toggles, CTC Mode
	TCCR1A = (0<<COM1A1)|(0<<COM1A0)|(0<<COM1B1)|(0<<COM1B0)|(0<<WGM11)|(0<<WGM01);
	//CTC Mode, N=64
	TCCR1B = (0<<ICNC1)|(0<<ICES1)|(0<<WGM13)|(1<<WGM12)|(0<<CS12)|(1<<CS11)|(1<<CS10);
	//Enable Channel A Match Interrupt
	TIMSK1 = (0<<ICIE1)|(0<<OCIE1B)|(1<<OCIE1A)|(0<<TOIE1);
	//Setup output compare for channel A
	OCR1A = 20000; //OCR1A = 16Mhz/N*Deltat-1 = 16Mhz/64*.1-1
	
	//**************************************************************************************
	//Configure Timer 0
	//**************************************************************************************
	TCCR0A = (1<<COM0A1)|(0<<COM0A0)|(1<<COM0B1)|(0<<COM0B0)|(1<<WGM01)|(1<<WGM00); //Setup Fast PWM Mode for Channel A
	TCCR0B = (0<<WGM02)|(1<<CS02)|(0<<CS01)|(1<<CS00); //Define Clock Source Prescaler
	OCR0A = 0; //Initialized PWM Duty Cycle
	OCR0B = 0;
	
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






//Interrupt Routine for A/D Completion
ISR (ADC_vect)
{
	low_out1 = ADCL;
	high_out1 = ADCH;
	low_out1 = (low_out1>>2);
	high_out1 = (high_out1<<6);
	if (left0right1 == 1) // right sensor
	{
		sensor_outright = (low_out1|high_out1);
		//sensor_outright = 0;
		left0right1++;
	}
	else if (left0right1 == 2) // front sensor
	{
		sensor_outfront = (low_out1|high_out1);
		//sensor_outright = 0;
		left0right1++;		
	}
	else if (left0right1 == 3) // back sensor
	{
		sensor_outback = (low_out1|high_out1);
		//sensor_outright = 0;
		left0right1 = 0;	
	}
	else // left sensor
	{
		sensor_outleft = (low_out1|high_out1);
		//sensor_outleft = 700;
		left0right1++;
	}
}

void turn_right() // ocr0a is right wheel, ocr0b is left wheel
{
	PORTB = (1<<PORTB0)|(0<<PORTB1)|(0<<PORTB2)|(1<<PORTB3); // Right: B1, B0 - Left: B2, B3
	OCR0A = 0;
	OCR0B = 50000; // shouldn't move
}

void turn_left()
{
	PORTB = (0<<PORTB0)|(1<<PORTB1)|(0<<PORTB2)|(1<<PORTB3); // Right: B1, B0 - Left: B2, B3
	OCR0A = 50000; // shouldn't move
	OCR0B = 0;
}

void go_straight()
{
	PORTB = (1<<PORTB0)|(0<<PORTB1)|(0<<PORTB2)|(1<<PORTB3); // Right: B1, B0 - Left: B2, B3
	OCR0A = OCR0B = 50000;
}

void go_back()
{
	PORTB = (0<<PORTB0)|(1<<PORTB1)|(1<<PORTB2)|(0<<PORTB3); // Right: B1, B0 - Left: B2, B3
	OCR0A = OCR0B = 50000;
}

void oscillation_check(int LTurn, int currTurn)
{
	if ((LTurn|currTurn) == 0x03)
	{
		osc_count++;
	}
	else
	{
		osc_count = 0;
	}
	if (osc_count > 10)
	{
		e_count = 50;
	}
	// content
}


//Interrupt Routine for Timer 1 Match
ISR (TIMER1_COMPA_vect)
{
	display_counter++;
	if (b_count > 0) // currently going backwards
	{
		if (sensor_outback > thresh_back){b_count=29;}
		else if (b_count < 30)
		{
			turn_right();
		}
		b_count--;
	}
	else // not currently going backwards
	{
		if (e_count > 0) // escaping a dead end
		{
			turn_left();
			e_count--;
		}
		else if ((sensor_outfront > thresh) & (sensor_outleft > thresh) & (sensor_outright > thresh))
		{
			go_back();
			b_count = 69;
			lastTurn = 0x00;
		}
		else // not escaping a dead end
		{
			//if ((PIND&(1<<PIND3))|(PIND&(1<<PIND2))) // bump sensor activated
			if (0)
			{
				go_back();
				b_count = 69;
				lastTurn = 0x00;
			}
			// maybe else if both sensors are active here
			else if (sensor_outleft > thresh) // left sensor sees obstacle
			{
				oscillation_check(lastTurn, 0x01);
				turn_right();
				lastTurn = 0x01;
			}
			else // no bump sensor and no left sensor activated
			{
				if (sensor_outright > thresh) // right sensor sees obstacle
				{
					oscillation_check(lastTurn, 0x02);
					turn_left();
					lastTurn = 0x02;
				}
				else if (sensor_outfront > thresh)
				{
					go_back();
					b_count = 69;
					lastTurn = 0x00;
				}
				else
				{
					go_straight();
					lastTurn = 0x00;
				}
				
			}
		}
	}
	
	if (display_counter>10){
			
			display_counter = 0;
			combined = sensor_outleft;
			
			FirstLineStr[4] = (combined/1000)+48;
			combined = combined % 1000;
			FirstLineStr[5] = (combined/100)+48;
			combined = combined % 100;
			FirstLineStr[6] = (combined/10)+48;
			combined = combined % 10;
			FirstLineStr[7] = combined+48;
			
			combined = sensor_outright;
			// combined = (combined/1023)*100;
			
			SecondLineStr[4] = (combined/1000)+48;
			combined = combined % 1000;
			SecondLineStr[5] = (combined/100)+48;
			combined = combined % 100;
			SecondLineStr[6] = (combined/10)+48;
			combined = combined % 10;
			SecondLineStr[7] = combined+48;
			
			if ((PIND&(1<<PIND3)) | (PIND&(1<<PIND2))){ThirdLineStr[8] = 1+48;}
			else {ThirdLineStr[8] = 0+48;}
			
			if (lastTurn == 0x02) // turning left
			{
				ThirdLineStr[3] = 0+48;
				FourthLineStr[3] = 1+48;
			}
			else if (lastTurn == 0x01) // turning right
			{
				ThirdLineStr[3] = 1+48;
				FourthLineStr[3] = 0+48;
			}
			else // both wheels going
			{
				ThirdLineStr[3] = 1+48;
				FourthLineStr[3] = 1+48;	
			}
			
			// h-bridge values
			if (PORTB&(1<<PORTB0)){FirstLineStr[9] = 1+48;}
			else {FirstLineStr[9] = 0+48;}
			if (PORTB&(1<<PORTB1)){FirstLineStr[11] = 1+48;}
			else {FirstLineStr[11] = 0+48;}
			if (PORTB&(1<<PORTB2)){SecondLineStr[9] = 1+48;}
			else {SecondLineStr[9] = 0+48;}
			if (PORTB&(1<<PORTB3)){SecondLineStr[11] = 1+48;}
			else {SecondLineStr[11] = 0+48;}
			
			// update LCD here
			display_state = 0;
			if(display_state==0)
			{
				UpdateTWIDisplayState();
			}
			
		}
	
	
	if (left0right1 == 1) // set which a2d pin to convert - right
	{
		ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(1<<MUX1)|(0<<MUX0);
	}
	else if (left0right1 == 2) // front sensor
	{
		ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(0<<MUX1)|(1<<MUX0);
	}
	else if (left0right1 == 3) // back sensor
	{
		ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(0<<MUX1)|(0<<MUX0);
	}
	else // left sensor
	{
		ADMUX = (0<<REFS1)|(1<<REFS0)|(0<<ADLAR)|(0<<MUX3)|(0<<MUX2)|(1<<MUX1)|(1<<MUX0);
	}
	
	ADCSRA |= (1<<ADSC); //Start Conversion

	
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
}
