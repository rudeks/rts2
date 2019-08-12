/**
 * IRF90 Rotating Focuser from Planewave Instruments.
 *
 * Copyright (C) 2019 Michael Mommert <mommermiscience@gmail.com>
 * based on Hedrick Focuser driver (planewave.cpp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "focusd.h"
#include "connection/serial.h"

// planewave leftovers
#define FOCUSSCALE       11.513442 
#define FOCUSDIR            1

#define FOCUSER_TICKS_PER_MICRON  207.288

//#define FOCUSERPORT "/dev/IRF" 

namespace rts2focusd
{

/**
 * Class for planewave IRF90 focuser. 
 *
 * @author Michael Mommert <mommermiscience@gmail.com>
 * @author Petr Kubanek <petr@kubanek.net>
 * @author Shashikiran Ganesh <shashi@prl.res.in>
 * 
 */
class PWIIRF90:public Focusd
{
	public:
		PWIIRF90 (int argc, char **argv);
		~PWIIRF90 (void);
		virtual int init ();
		virtual int setTo (double num);
		virtual double tcOffset () {return 0.;};
		virtual int isFocusing ();
		virtual int info ();

	protected:
		virtual int processOption (int opt);
		virtual int initValues ();

		virtual int setValue (rts2core::Value *old_value, rts2core::Value *new_value);

		virtual bool isAtStartPosition () { return false; }

	private:
		void getValue(const char *name, rts2core::Value *val);
		void openRem ();
		int findOptima ();
		
		int getPos ();
		int getTemp ();
		int setFan (int fancmod);
		int GetTemperature(double *teltemperature, int tempsensor);
		int focusdir;  /*  use -1 if focuser moves in opposite direcion */
		
		int SetHedrickFocuser (int focuscmd, int focusspd);
		
		char buf[15];
		const char *device_file;
		
		rts2core::ConnSerial *sconn;
		rts2core::ValueFloat *TempM1;
		rts2core::ValueFloat *TempAmbient;
		rts2core::ValueBool *fanMode;

};

}

using namespace rts2focusd;

PWIIRF90::PWIIRF90 (int argc, char **argv):Focusd (argc, argv)
{
	sconn = NULL;
	device_file = "/dev/IRF";  // Lowell IRF90 default

	focType = std::string ("PWIIRF90");
	createTemperature ();

	createValue (TempM1, "temp_primary",
				 "[C] Primary mirror temperature", true);
	createValue (TempAmbient, "temp_ambient",
				 "[C] Ambient temperature in telescope", true);

	createValue (fanMode, "FANMODE", "Fan ON? : TRUE/FALSE", false, RTS2_VALUE_WRITABLE);
		
	setFocusExtent (-1000, 30000);
	
	addOption ('f', NULL, 1, "device file (usually /dev/IRF)");
}

PWIIRF90::~PWIIRF90 ()
{
	//delete sconn;
}

int PWIIRF90::processOption (int opt)
{
	switch (opt)
	{
		case 'f':
			device_file = optarg;
			break;
		default:
			return Focusd::processOption (opt);
	}

	return 0;
}

int PWIIRF90::initValues ()
{
	focusdir = 1; /* use -1 if focuse moves in opposite direction */
	
	return Focusd::initValues ();
}

int PWIIRF90::setValue (rts2core::Value *old_value, rts2core::Value *new_value)
{
	if (old_value == fanMode)
	{
		setFan (new_value->getValueInteger());
		return 0;
	}

  	return Focusd::setValue (old_value, new_value);
}

int PWIIRF90::setFan(int fancmd)
{
	/* Default fan string for off */
	
	char outputstr[] = { 0x50, 0x02, 0x13, 0x27, 0x00, 0x00, 0x00, 0x00 };
	char returnstr[] = {0x00, 0x00, 0x00, 0x00 };
	
	
	/* Request the fans on through the EFA on */
	
	if ( fancmd > 0 )
	{
		outputstr[4] = 0x01;
	} 

	logStream (MESSAGE_DEBUG) << " fan cmd is " << fancmd << sendLog;
	
	/* Send the command */
	sconn->writeRead(outputstr, 8, returnstr, 4);
	if (returnstr[0] != '#') 
	    logStream (MESSAGE_ERROR) << " No acknowledgement from thermal control" << sendLog;
	    
	return 0;
}    

int PWIIRF90::GetTemperature(double *temp, int tempsensor=0)
{
	std::stringstream ss;
	std::string _buf;
	int i;
	unsigned int chksum = 0;
	unsigned char sendstr[] = { 0x3b, 0x04, 0x20, 0x12, 0x26, 0x00, 0x00 };
	char returnstr[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	// have to update sendstr[5] (sensor) and sendstr[6] (checksum)
	sendstr[5] = tempsensor;
	
	// derive and update checksum byte
	// exclude start of message and chksum bytes
	for (i=1; i<(sizeof(sendstr)/sizeof(char)-1); i++)
		chksum += (unsigned int)sendstr[i];
	chksum = -chksum & 0xff;
	sendstr[6] = chksum;

	// debug only
	// for (i=0; i<sizeof(sendstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)sendstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "send command "  << _buf << sendLog;
	
	// send command
	sconn->flushPortIO();
	sconn->writePort((const char*)(&sendstr), sizeof(sendstr)/sizeof(char));

	// read acknowledgement
	if (sconn->readPort(returnstr, 7) == -1)
	{
		logStream (MESSAGE_ERROR) << "could not read from serial connection"
								  << sendLog;
		return -1;
	}

	// debug only
	// ss.str(std::string());
	// for (i=0; i<sizeof(returnstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)returnstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "received acknowledgement "
	//                           << _buf << sendLog;

	// switch RTS bit to receive results
	sconn->switchRTS();

	// read reply
	if (sconn->readPort(returnstr, sizeof(returnstr)/sizeof(char)) == -1)
	{
		sconn->switchRTS();
		return -1;
	}

	// check checksum
	chksum = 0;
	for (i=1; i<(sizeof(returnstr)/sizeof(char)-1); i++)
		chksum += (unsigned int)returnstr[i];
	chksum = -chksum & 0xff;

	// // debug only
	// ss.str(std::string());
	// for (i=0; i<sizeof(returnstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)returnstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "received result "  << _buf << sendLog;
	
	if ((int)(unsigned char)returnstr[7] != chksum)
	{
		sconn->switchRTS();
		logStream (MESSAGE_ERROR) << "result checksum corrupt" << sendLog;
		return -1;
	}
	
	// switch RTS bit to enable requests again
	sconn->switchRTS();

	// calculate temperature from result
	float _temp = (256*(int)(unsigned char)returnstr[6] +
				  (int)(unsigned char)returnstr[5]) / 16.0;

	*temp = _temp;
	
	// logStream (MESSAGE_DEBUG) << "resulting temperature " << _temp <<
	// 	sendLog;
	
	return 0;
}
 
int PWIIRF90::SetHedrickFocuser(int focuscmd, int focusspd)
{
	logStream (MESSAGE_ERROR) << "in set hedrick" << sendLog;

	char sendstr[] = { 0x50, 0x02, 0x12, 0x24, 0x01, 0x00, 0x00, 0x00 };
	char returnstr[2048];
	
	/* Set the speed */

	sendstr[4] = 0x08;

	if (focusspd >= 4)
	{
		sendstr[4] = 0x08;
	}
	
	if (focusspd == 3)
	{
		sendstr[4] = 0x06;
	}
	
	if (focusspd == 2)
	{
		sendstr[4] = 0x04;
	}
	
	if (focusspd == 1)
	{
		sendstr[4] = 0x02;
	}
	  
	if ( focuscmd == -1 )
	{
		/* Run the focus motor in */
		/* Set the direction based on focusdir */
		
		if (focusdir > 0)
		{
			sendstr[3] = 0x25;
		}
		else
		{
			sendstr[3] = 0x24;
		}
			      
		/* Send the command */
		/* Wait up to a second for an acknowledgement */
		
		for (;;) 
		{
			if ( sconn->writeRead(sendstr,8,returnstr,1) ) 
			{
				if (returnstr[0] == '#')
					break;
			}
			else 
			{ 
				fprintf(stderr,"No acknowledgement from focuser\n");
			}
		}	    
	}
	
	if ( focuscmd == 1 )
	{  
		/* Run the focus motor out */
		/* Set the direction based on focusdir */
		if (focusdir > 0)
		{
			sendstr[3] = 0x24;
		}
		else
		{
			sendstr[3] = 0x25;
		}
	      
		/* Send the command */
		/* Wait up to a second for an acknowledgement */
	  
		for (;;) 
		{
			if (sconn->writeRead(sendstr,8,returnstr,1))
			{
				if (returnstr[0] == '#')
					break;
			}
			else 
			{ 
				fprintf(stderr,"No acknowledgement from focus control\n");
			}
		}      
	}
	 
	if (focuscmd == 0)
	{
		/* Set the speed to zero to stop the motion */
	  
		sendstr[4] = 0x00;
	  
		/* Send the command */
		/* Wait up to a second for an acknowledgement */
	  
		for (;;) 
		{
			if (sconn->writeRead(sendstr,8,returnstr,1))
			{
				if (returnstr[0] == '#')
					break;
			}
			else 
			{ 
				fprintf(stderr,"No acknowledgement from focuser\n");
			}
		}            
	}  
	return 0;
}


// send focus to given position
//
int PWIIRF90::setTo (double num)
{
	//logStream (MESSAGE_DEBUG) << " in setTo " << sendLog ;  

	target->setValueDouble(num);

	return 0;
}

int PWIIRF90::isFocusing ()
{
	float pos_diff =position->getValueDouble() - target->getValueDouble();
	float abspos_diff = fabs(pos_diff);
	int focspeed = 4;
	int focdir = 0;

	/* Focus speed values   */
	/*                      */
	/* Fast     4           */
	/* Medium   3           */
	/* Slow     2           */
	/* Precise  1           */
  
	/* Focus command values */
	/*                      */
	/* Out     +1           */
	/* In      -1           */
	/* Off      0           */

	pos_diff = position->getValueDouble() - target->getValueDouble();
	abspos_diff = fabs(pos_diff);

	if (abspos_diff <= 1)
	{
		SetHedrickFocuser(0,0);
		return -2;
	}

	//  focuser is not at target - so move the focuser here
	focspeed = 4;   // assume it is at large deviation and needs highest speed... 

	if (pos_diff > 0)
		focdir = -1;
	if (pos_diff < 0)
		focdir = 1;

	if (abspos_diff < 1000)
		focspeed = 3;
		
	if (abspos_diff < 50)
		focspeed = 2;
		
	if (abspos_diff < 10)
		focspeed = 1;
		
	if (abspos_diff <= 1)
	{
		focspeed = 0;
		focdir = 0;
		abspos_diff = 0;
	}
				
	//	logStream (MESSAGE_DEBUG) << "changing position from " << position->getValueDouble () << " to " << target->getValueDouble() << sendLog;
	//	logStream (MESSAGE_DEBUG) << "focus speed " << focspeed << " and focus direction " << focdir << sendLog;
	//	logStream (MESSAGE_DEBUG) << "abspos_diff " << abspos_diff << " pos_diff " << pos_diff << sendLog;
		
	SetHedrickFocuser (focdir, focspeed);  // for a fixed duration 
	getPos ();   // get new position 

	if (fabs(target->getValueInteger () - position->getValueInteger ()) <= 1)
	{
		SetHedrickFocuser(0,0);
		return -2;
	}
	return 0;
}

/* 
    this is based on the logic in atc2.cpp - i.e. we ensure that the focuser is connected and switched on 
    - code suitably modified for hedrick focuser commands 
    - basically ask for version number and ensure number of returned bytes and last char is \#...
    
    */
void PWIIRF90::openRem ()
{
	unsigned char sendStr[] = { 0x3b, 0x03, 0x20, 0xfe, 0x00, 0x00, 0x00, 0x02 };

	int ret = sconn->writeRead ((char *) sendStr, 8, buf, 3);
	if (ret != 3)
		throw rts2core::Error ("focuser open command didn't respond with 3 chars ");


	std::stringstream ss;
	int i = 0;
	for (i=0; i<3; ++i)
		ss << std::hex << (int)buf[i] << " ";
	std::string _buf = ss.str();
		
	logStream (MESSAGE_DEBUG) << "_buf "  << buf << sendLog;
	
	


	
	// if (buf[2] != 0x23 )
	//         throw rts2core::Error (std::string ("invalid reply from planewave focuser getVersion command :")+ buf);
	// logStream (MESSAGE_DEBUG) << "focuser responded with " << buf <<  " EOL " << sendLog ;
	
}


int PWIIRF90::getTemp ()
{
	double temp; 

	// primary mirror
	if (GetTemperature(&temp, 0) < 0)
	{
		logStream (MESSAGE_ERROR) <<
			"could not read primary mirror temperature" <<  sendLog;
		return -1;
	}
	TempM1->setValueFloat(temp);
	temperature->setValueFloat(temp);
    // FOC_TEMP is Primary Mirror Temperature ... 

	logStream (MESSAGE_DEBUG) << "primtemp " << temp <<  sendLog;

	
	// ambient temperature
	if (GetTemperature(&temp, 1) < 0)
	{
		logStream (MESSAGE_ERROR) <<
			"could not read ambient temperature" <<  sendLog;
		return -1;
	}
	TempAmbient->setValueFloat(temp);

	return 0;
}

int PWIIRF90::info ()
{
	getPos();
	getTemp (); 

	return Focusd::info ();
	
}

int PWIIRF90::getPos ()
{
	std::stringstream ss;
	std::string _buf;
	int i;
	unsigned int chksum = 0;
	unsigned char sendstr[] = { 0x3b, 0x03, 0x20, 0x12, 0x1, 0xca };
	char returnstr[] = { 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00 };
	
	// debug only
	// for (i=0; i<sizeof(sendstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)sendstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "send command "  << _buf << sendLog;

	// send command
	sconn->flushPortIO();
	sconn->writePort((const char*)(&sendstr), sizeof(sendstr)/sizeof(char));

	// read acknowledgement
	if (sconn->readPort(returnstr, sizeof(sendstr)/sizeof(char)) == -1)
	{
		logStream (MESSAGE_ERROR) << "could not read from serial connection"
								  << sendLog;
		return -1;
	}

	// debug only
	// ss.str(std::string());
	// for (i=0; i<sizeof(returnstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)returnstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "received acknowledgement "
	//                           << _buf << sendLog;

	// switch RTS bit to receive results
	sconn->switchRTS();

	// read reply
	if (sconn->readPort(returnstr, sizeof(returnstr)/sizeof(char)) == -1)
	{
		sconn->switchRTS();
		return -1;
	}

	// check checksum
	chksum = 0;
	for (i=1; i<(sizeof(returnstr)/sizeof(char)-1); i++)
		chksum += (unsigned int)returnstr[i];
	chksum = -chksum & 0xff;

	// // debug only
	// ss.str(std::string());
	// for (i=0; i<sizeof(returnstr)/sizeof(char); ++i)
	// 	ss << std::hex << (int)returnstr[i] << " ";
	// _buf = ss.str();
	// logStream (MESSAGE_DEBUG) << "received result "  << _buf << sendLog;
	
	if ((int)(unsigned char)returnstr[8] != chksum)
	{
		sconn->switchRTS();
		logStream (MESSAGE_ERROR) << "result checksum corrupt" << sendLog;
		return -1;
	}
	
	// switch RTS bit to enable requests again
	sconn->switchRTS();

	int b0 = (unsigned char) returnstr[5];
  	int b1 = (unsigned char) returnstr[6];
  	int b2 = (unsigned char) returnstr[7];
  	float focus = (256*256*b0 + 256*b1 + b2)/FOCUSER_TICKS_PER_MICRON;

	// logStream (MESSAGE_DEBUG) << "resulting focus position " << focus <<
	//  	sendLog;
	
	position->setValueInteger(focus);
	sendValueAll(position);
	return 0;
	
}



int PWIIRF90::init ()
{
	int ret;
	ret = Focusd::init ();
	
	if (ret) 
		return ret;
	
	sconn = new rts2core::ConnSerial(device_file, this, rts2core::BS19200,
									 rts2core::C8, rts2core::NONE, 5);
	sconn->setDebug(getDebug());
	sconn->init();

	sconn->flushPortIO();

	/* communications protocol:
	   ========================
	   messages are hex of the form:
	   { 0x3b (start of message byte), 
	     number of bytes (excluding 0x3b, nob, and checksum),
         source address,
         receiver address,
		 command,
		 *data,
		 checksum
       }

	   # Device addresses recognized by IRF90
	   (EFA is an electronic focus something, not important here)

	   Address = Enum(
		   PC = 0x20,       # User's computer
		   HC = 0x0D,       # EFA hand controller
		   FOC_TEMP = 0x12, # EFA focus motor and temp sensors
		   ROT_FAN = 0x13,  # EFA rotator motor and fan control
		   DELTA_T = 0x32   # Delta-T dew heater
	   )

	   # Temperature sensors.
	   # NOTE: Not all telescope configurations include all temp sensors!
	   TempSensor = Enum(
		   PRIMARY = 0,
		   AMBIENT = 1,
		   SECONDARY = 2,
		   BACKPLATE = 3,
		   M3 = 4
	   )

	   # Commands recognized by the EFA. See protocol documentation for more details.
	   Command = Enum(
		   MTR_GET_POS = 0x01,
		   MTR_GOTO_POS2 = 0x17,
		   MTR_OFFSET_CNT = 0x04,
		   MTR_GOTO_OVER = 0x13,
		   MTR_PTRACK = 0x06,
		   MTR_NTRACK = 0x07,
		   MTR_SLEWLIMITMIN = 0x1A,
		   MTR_SLEWLIMITMAX = 0x1B,
		   MTR_SLEWLIMITGETMIN = 0x1C,
		   MTR_SLEWLIMITGETMAX = 0x1D,
		   MTR_PMSLEW_RATE = 0x24,
		   MTR_NMSLEW_RATE = 0x25,
		   TEMP_GET = 0x26,
		   FANS_SET = 0x27,
		   FANS_GET = 0x28,
		   MTR_GET_CALIBRATION_STATE = 0x30,
		   MTR_SET_CALIBRATION_STATE = 0x31,
		   MTR_GET_STOP_DETECT = 0xEE,
		   MTR_STOP_DETECT = 0xEF,
		   MTR_GET_APPROACH_DIRECTION = 0xFC,
		   MTR_APPROACH_DIRECTION = 0xFD,
		   GET_VERSION = 0xFE,

	   )
	*/
	
	return 0;	
}


int main (int argc, char **argv)
{
	PWIIRF90 device (argc, argv);
	return device.run ();
}
