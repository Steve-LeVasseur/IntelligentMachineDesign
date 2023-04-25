/*
 * IMD_Demo2_LCD_BlockingCode.cpp
 *
 * Created: 2/13/2022 2:47:06 PM
 * Author : Zach
 */ 
#include <avr/io.h>
#include <avr/interrupt.h>
//Define the codes for TWI actions
#define TWCR_START 0xA4 //send start condition
#define TWCR_STOP 0x94 //send stop condition
#define TWCR_RSTART 0xA4 //send repeated start
#define TWCR_RACK 0xC4 //receive byte and return ack to slave
#define TWCR_RNACK 0x84 //receive byte and return nack to slave
#define TWCR_SEND 0x84 //pokes the TWINT flag in TWCR and TWEN
//Function Prototypes
void InitializeDisplay();
void UpdateDisplay();
void TWI_Start();
void TWI_SendAddress(uint8_t);
void TWI_SendData(uint8_t);
void TWI_Stop();
void DummyLoop(uint16_t);
volatile uint8_t FirstLineStr[21] =  "First Line          ";
volatile uint8_t SecondLineStr[21] = "Second Line         ";
volatile uint8_t ThirdLineStr[21] =  "Third Line          ";
volatile uint8_t FourthLineStr[21] = "Fourth Line         ";
int main(void)
{
    //Configure I/O
    //Set SDA (PC4) and SCL (PC5) as outputs for TWI bus
    DDRC = (1<<PORTC5)|(1<<PORTC4);
    //Configure PD0 as output for debug output
    DDRD = (1<<PORTD0)|(1<<PORTD1)|(1<<PORTD2)|(1<<PORTD3);
    //Configure Timer 1 to generate an ISR every 100ms
    //No Pin Toggles, CTC Mode
    TCCR1A = (0<<COM1A1)|(0<<COM1A0)|(0<<COM1B1)|(0<<COM1B0)|(0<<WGM11)|(0<<WGM01);
    //CTC Mode, N=64
    TCCR1B = (0<<ICNC1)|(0<<ICES1)|(0<<WGM13)|(1<<WGM12)|(0<<CS12)|(1<<CS11)|(1<<CS10);
    //Enable Channel A Match Interrupt
    TIMSK1 = (0<<ICIE1)|(0<<OCIE1B)|(1<<OCIE1A)|(0<<TOIE1);
    //Setup output compare for channel A
    OCR1A = 24999; //OCR1A = 16Mhz/N*Deltat-1
    
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
    TWCR = (0<<TWINT)|(0<<TWEA)|(0<<TWSTO)|(0<<TWWC)|(1<<TWEN)|(0<<TWIE);
    //Configure TWI Status Register (TWSR)
    //TWS7-TWS3: 00000 - Don't change status
    //TWPS1-TWPS0: 10 - Prescaler Value = 64
    TWSR = (0<<TWS7)|(0<<TWS6)|(0<<TWS5)|(0<<TWS4)|(0<<TWS3)|(1<<TWPS1)|(0<<TWPS0);
	InitializeDisplay();
    sei();
    /* Replace with your application code ----------------------------------------------------------------------------------------------- */
    while (1) 
    {
        PIND = (1<<PIND0); // Maybe add ^ to make it toggle I guess
    }
}
ISR(TIMER1_COMPA_vect)
{
    //Start New LCD Screen Update
//PORTD ^= (1<<PORTD3);
    UpdateDisplay();
}
/************************************************************************/
/* Display Functions                                                    */
/************************************************************************/
void UpdateDisplay()
{
    //Update the first and third line
    TWI_Start();//Create start signal
    TWI_SendAddress(0x78); //Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0x80); //Send Instruction to set DDRAM Address to 00
    TWI_SendData(0x40); //Send control byte for DDRAM data
    //Loop through each character of the first line
    for (uint8_t i=0;i<20;i++)
    {
        TWI_SendData(FirstLineStr[i]); //Send the next character
    }
    //Loop through each character of the third line
    //Note: remember third line address starts after first line)
    for (uint8_t i=0;i<20;i++)
    {
        TWI_SendData(ThirdLineStr[i]); //Send the next character
    }
    TWI_Stop(); //Send the stop condition
    //Update the second and fourth line
    TWI_Start();//Create start signal
    TWI_SendAddress(0x78); //Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0xC0); //Send Instruction to set DDRAM Address to 40
    TWI_SendData(0x40); //Send control byte for DDRAM data
    //Loop through each character of the first line
    for (uint8_t i=0;i<20;i++)
    {
        TWI_SendData(SecondLineStr[i]); //Send the next character
    }
    //Loop through each character of the third line
    //Note: remember third line address starts after first line)
    for (uint8_t i=0;i<20;i++)
    {
        TWI_SendData(FourthLineStr[i]); //Send the next character
    }
    TWI_Stop(); //Send the stop condition
}
void InitializeDisplay()
{
    //Wait for 40ms to give LCD time to power up
    DummyLoop(400);
    //Function Set
    TWI_Start();//Create Start Signal
    TWI_SendAddress(0x78);//Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0x38); //Set Function Mode
    TWI_Stop();
    DummyLoop(1);//Delay 100us
    TWI_Start();//Create Start Signal
    TWI_SendAddress(0x78);//Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0x0C); //Turn on Display, Cursor, and Blink
    DummyLoop(1);//Delay 100us
    TWI_Start();//Create Start Signal
    TWI_SendAddress(0x78);//Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0x01); //Clear Display
    TWI_Stop();
    DummyLoop(100);//Delay 10ms
    TWI_Start();//Create Start Signal
    TWI_SendAddress(0x78);//Send LCD Slave Address
    TWI_SendData(0x80); //Send control byte for instruction
    TWI_SendData(0x01); //Set Entry Mode
    TWI_Stop();
    DummyLoop(1);//Delay 100us
}
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
/************************************************************************/
/* Define TWI Functions                                                 */
/************************************************************************/
//Create start condition on TWI bus
void TWI_Start()
{
    TWCR = TWCR_START;  //send start condition
    while(!(TWCR & (1<<TWINT))){PIND = (1<<PIND2);} //wait for start condition to transmit
}
//Send Address Packet
void TWI_SendAddress(uint8_t address)
{
    TWDR = address;  //send address to get device attention
    TWCR = TWCR_SEND;  //Set TWINT to send address
    while(!(TWCR & (1<<TWINT))){PIND = (1<<PIND2);} //wait for address to go out
}
//Send Data Packet
void TWI_SendData(uint8_t data)
{
    TWDR = data;//send data to address
    TWCR = TWCR_SEND; //Set TWINT to send address
    while(!(TWCR & (1<<TWINT))){PIND = (1<<PIND2);} //wait for data byte to transmit
}
//Create stop condition
void TWI_Stop()
{
    TWCR = TWCR_STOP;//finish transaction
}

