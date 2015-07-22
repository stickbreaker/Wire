/*
Example I2C Master / Slave combination  Sends and receives a datapacket of 8 bytes,
  one long, one float.  
  
  uses address =0x58, 0x59 for I2C addresses
    Selectable by grounding A1
	
  Grounding A0 cause continous 1 second Transmit, Receive, increment of (fnum,foo) to slave.
  
  hook up 2 arduino's 
    SCL - SCL,
	SDA - SDA,
	Gnd - Gnd.
	place a 4.7k pullup resistor from SCL to +5v,
    place a 4.7k pullup resistor from SDA to +5V
    connect A1 on ONE of the Arduino's to Gnd, this will assign 0x58 as SLAVE I2C address
	
	Load this sketch into both Adruino's
	
	the Serial Monitor controls are:
		1 : Display fnum, foo
		2 : Read From other Slave
		5 : Write to  other Slave
		7 : Inc fnum
		8 : inc foo
		S : Scan I2C buss
		ground A0 to auto send/rcv other Slave
	
*/
#include <Wire.h>
#include <avr/pgmspace.h>
#include <compat/twi.h> // for TW constants

volatile long fnum;   // volatile Need because variable will be accessed during an interrupt ISR
volatile float foo;   //  same thing.
volatile uint8_t newData; // flag to detect I2C Slave Mode activity


#define DATALEN (sizeof fnum) + (sizeof foo)

#define SLAVE_ADDRESS_1 0x58   // slave 1e
#define SLAVE_ADDRESS_2 0x59   // slave 2

void help(){
char m0[] PROGMEM={"Valid Commands"};
char m1[] PROGMEM={"1 : Display fnum, foo"};
char m2[] PROGMEM={"2 : Read From other Slave"};
char m3[] PROGMEM={"5 : Write to  other Slave"};
char m4[] PROGMEM={"7 : Inc fnum"};
char m5[] PROGMEM={"8 : inc foo"};
char m6[] PROGMEM={"S : Scan I2C buss"};
char m7[] PROGMEM={"ground A0 to auto send/rcv other Slave"};
Serial.println(m0);
Serial.println(m1);
Serial.println(m2);
Serial.println(m3);
Serial.println(m4);
Serial.println(m5);
Serial.println(m6);
Serial.println(m7);
}

void onRequestEvent(void){
   I2C_writeAnything(fnum);
   I2C_writeAnything(foo);
   newData = newData | 1; // set bit 0 to mark I2C Sent data
}

void onReceiveEvent(int numbytes){
if(numbytes == DATALEN){ // Masters beginTransmission(SLAVE_ADDRESS);Write();endTransmission();
   // sent the correct number of bytes
   I2C_readAnything(fnum);
   I2C_readAnything(foo);
   newData = newData | 2; // set bit 1 to mark I2C Received Data.
   }
}

uint8_t I2C_writeError(uint8_t ec,uint8_t ign=0){
switch (ec){
	case 0 : break; // no error
	case 1 : if(!(ign&1))Serial.println(F("length to long for buffer"));
	  break;
	case 2 : if(!(ign&2))Serial.println(F("address send, NACK received"));
	  break;
	case 3 : if(!(ign&4))Serial.println(F("data send, NACK received"));
	  break;
	case 4 : if(!(ign&8)){
		if(Wire.lastError()==TW_BUS_ERROR)Serial.println(F("I2C Timeout"));
		else Serial.println(F("other twi error (lost bus arbitration,.."));
	}
      break;	
	default : break;
}
return ec;
}

uint8_t I2C_readError(uint8_t ec,uint8_t ign=0){
if (!(ign&1)&&(ec==0)){ // no data returned
  if(Wire.lastError()==TW_BUS_ERROR)Serial.println(F("I2C Timeout"));
  }
return ec;
}


void scan(){
Serial.println(F(" Scanning I2C Addresses"));
uint8_t cnt=0;
for(uint8_t i=0;i<128;i++){
  Wire.beginTransmission(i);
  uint8_t ec=I2C_writeError(Wire.endTransmission(true),2);
  if(ec==0){
    if(i<16)Serial.print('0');
    Serial.print(i,HEX);
    cnt++;
  }
  else if(Wire.lastError()==TW_BUS_ERROR) break;
  else 	
	  Serial.print(F(".."));
  Serial.print(' ');
  if ((i&0x0f)==0x0f)Serial.println();
  }
Serial.print(F("Scan Completed, "));
Serial.print(cnt);
Serial.println(F(" I2C Devices found."));
}

uint8_t other,me;

void setup(){
  Serial.begin(9600); // if USB is connected to this slave and Serial monitor is on 
    //we will send debug messages.
  foo = 98.6;
  fnum = 7;
  newData = 0;
  pinMode(A0,INPUT_PULLUP);
  pinMode(A1,INPUT_PULLUP); // slave1 or slave2
  if(digitalRead(A1))other=SLAVE_ADDRESS_1;
  else other=SLAVE_ADDRESS_2;
  me = (other & 0xFE)+((~other)&1);
  Serial.print(F("other="));Serial.print(other,HEX);Serial.print(F(" me="));
  Serial.println(me,HEX);
  Wire.begin(me);  // also slave
  Wire.onReceive(onReceiveEvent);
  Wire.onRequest(onRequestEvent);
  
  }

void debug_out( char msg[]){
	Serial.println(msg);
	Serial.print(F("foo="));
    Serial.print(foo);
    Serial.print(F(" and fnum="));
    Serial.println(fnum,DEC);
	Serial.print(F(" Self SlaveID ="));
	Serial.print(me,HEX);
	Serial.print(F(" Other SlaveID="));
	Serial.println(other,HEX);
}  
  
  
void loop(){
  if (Serial.available()){ // keypressed from Serial Monitor
    char ch=Serial.read();
	switch (ch){
		case '1' : // Display Self info.
		  debug_out("About Me");
		  break;
		case '2' :  // send to other
		  Wire.beginTransmission(other);
		  I2C_writeAnything(fnum);
		  I2C_writeAnything(foo);
		  I2C_writeError(Wire.endTransmission(true));
		  debug_out("Sent Other Slave");
		  break;
		case '5' : // Receive from Other Slave
		  Wire.requestFrom(other,DATALEN,true);
		  if(Wire.available()==DATALEN){
			  I2C_readAnything(fnum);
			  I2C_readAnything(foo);
			  debug_out("Read from other Slave");
		  }
		  else debug_out("Tried to Read from other Slave, Failed.");
		  break;
		  
		case '7' : // inc fnum
		  fnum += 1;
		  Serial.println( fnum,DEC);
		  break;
		case '8' : // inc foo;
		  foo += 0.125;
		  Serial.println(foo);
		  break;
		case 'S' : scan();
		  break;
		default: Serial.print("Say What? ");
		  Serial.println(ch);
		  help();
	}
  }else{
	  if (digitalRead(A0)==LOW){ // automated send/ receive update so I could monitor SLAVE. Ground A0 to activate
		  delay(1000);
     	  Wire.beginTransmission(other);
		  I2C_writeAnything(fnum);
		  I2C_writeAnything(foo);
		  Wire.endTransmission(true);
          debug_out("sent");         
		  Wire.requestFrom(other,DATALEN);
		  if(Wire.available()==DATALEN){
			  I2C_readAnything(fnum);
			  I2C_readAnything(foo);
              debug_out("received");
		  }
		  else debug_out("Tried to Read from other Slave, Failed.");
		 foo = foo +0.1;
         fnum = fnum +2;
	  }
      if((newData&1)==1){ // I2C send data out
		Serial.print(F(" Self Slave Sent foo="));
		Serial.print(foo);
		Serial.print(F(" and Fnum="));
		Serial.println(fnum,DEC);
		newData = newData & B11111110; // Clear data sent marker
		}
	if((newData&2)==2){// I2C received Data
		Serial.print(F(" Self Slave Received foo="));
		Serial.print(foo);
		Serial.print(F(" and Fnum="));
		Serial.println(fnum,DEC);
		newData = newData & B11111101; // Clear data sent marker
		}
	  
  }	  
}