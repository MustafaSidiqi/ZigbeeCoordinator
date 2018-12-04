//Includes
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <math.h>
#include <sstream>

using namespace std;

//Defines
#define BAUDRATE B9600
#define MODEMDEVICE "/dev/ttyAMA0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 02
#define TRUE 1


//Global Variables
volatile int STOP = FALSE;
volatile int READ_STOP = FALSE;
volatile int READ_ERROR = FALSE;
volatile int fd, c, res;
struct termios oldtio, newtio;

char API_Start_delimitter = 0x7E;

char XBeeID_Coordinator[8] = { 0x00, 0x13, 0xA2, 0x00, 0x40, 0xE7, 0x1A, 0xEF };
char XBeeID_Router2[8] = { 0x00, 0x13, 0xA2, 0x00, 0x40, 0xE7, 0x1A, 0xE1 };
char XBeeID_End_One[8] = { 0x00, 0x13, 0xA2, 0x00, 0x41, 0x6B, 0x89, 0x45 };
char XBeeID_End_Two[8] = { 0x00, 0x13, 0xA2, 0x00, 0x41, 0x6B, 0x89, 0x63 };

char packageSend[50] = { };
char packageRead[50] = { };
int packageReadLength = 0;

//Functions
void initUART();
void receive();
void send(char Start_delimitter, char mode, char *XBeeID, char *ATCommand, char *ATParameters, char ATParametersOn);


int main()
{
	initUART();
	char par[4];

	while(1) {
		printf("---------- ADC SAMPLE: ----------\n");

		// send(0x7E, 0x17, XBeeID_End_One, "IS", par, 1); //forcesample
		receive();

		printf("%x \n", packageRead[packageReadLength - 15]);

		int ADCHumidity = (packageRead[packageReadLength - 5] << 8) + packageRead[packageReadLength - 4];
		int ADCTemperature = (packageRead[packageReadLength - 3] << 8) + packageRead[packageReadLength - 2];
		printf("\n Analog data: %d\n", ADCHumidity);
		printf("\n Analog data: %d\n", ADCTemperature);
		// 7E 00 16 92 00 13 A2 00 41 6B 89 63 C1 22 01 01 00 00 07 00 00 02 22 03 F7 16

		double R = -(25000 * (ADCTemperature - 1024)) / ADCTemperature;
		double Rref = 3103.0; 
		double A1 = 3.35016 * pow(10, -3);
		double B1 = 2.56985 * pow(10, -4);
		double C1 = 2.62013 * pow(10, -10); 
		double D1 = 6.38309 * pow(10, -15);
		double A = -14.6337; 
		double B = 4791.842; 
		double C = -115334.0; 
		double D0 = -3730535;

		double temperature = (1 / (A1 + B1 * log(R / Rref) + C1 * pow(log(R / Rref), 2) + D1 * pow(log(R / Rref), 3))) -273.15;
		printf("\n Temperature: %f\n", temperature);

		double HumA = -38.83;
		double HumB = 25000;
		double HumC = 587.71;
		double HumD = log(-(HumB * (ADCHumidity - 1024)) / ADCHumidity);

		double humidity = (HumA * HumD + HumC) / 3; 
		//string temp = temperature

		std::ostringstream T; 
		std::ostringstream H;
		std::ostringstream Room;
		std::ostringstream P;
		std::ostringstream L;


		T << temperature;
		H << humidity;
		Room << "X2.01";
		P << packageRead[packageReadLength - 15];
		L << "DTU";


		string command = "curl -d \"T=" + T.str() + "&H=" + H.str() + "&R=" + Room.str() + "&P=" + P.str() + "&L=" + L.str() + "\" -H \"Content-Type:application/x-www-form-urlencoded\" -X POST https://s164854-iot-project-dtu.eu-gb.mybluemix.net/test";
		cout << command; 

		printf("\n Humidity: %f\n", humidity);
		system(command.c_str());
	}
		tcsetattr(fd, TCSANOW, &oldtio);
	}

	//Init UART communication
	void initUART() {
		fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY);
		if (fd < 0) { perror(MODEMDEVICE); exit(1); }

		tcgetattr(fd, &oldtio); /* save current port settings */
		bzero(&newtio, sizeof(newtio));
		newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
		newtio.c_iflag = IGNPAR;
		newtio.c_oflag = 0;

			/* set input mode (non?canonical, no echo,...) */
		newtio.c_lflag = 0;
		newtio.c_cc[VTIME] = 0; /* inter?character timer unused */
		newtio.c_cc[VMIN] = 5; /* blocking read until 5 chars received */
		tcflush(fd, TCIFLUSH);
		tcsetattr(fd, TCSANOW, &newtio);
	}

	//Recive return Package
	void receive()
	{
		printf("\nStart Reading... \n");
		char readTemp[1] = { };
		int r = 0;
	
		while (READ_STOP == FALSE) {
			//Read 1. byte
			r = read(fd, readTemp, 1);
			if (r != 1) READ_ERROR = TRUE;
			packageRead[0] = readTemp[0];

					//If 1. byte = startbyte 0x7e
			if (packageRead[0] == 0x7e) {
				printf("Package found! \n");
			
				READ_STOP = TRUE;

							//Read length of package
				r = read(fd, readTemp, 1);
				if (r != 1) READ_ERROR = TRUE;
				packageRead[1] = readTemp[0];

				r = read(fd, readTemp, 1);
				if (r != 1) READ_ERROR = TRUE;
				packageRead[2] = readTemp[0];

							//Calculating length of package
				int length = (packageRead[1] << 8) + packageRead[2];
				packageReadLength = length + 4;
				printf("Package length: %d\n\n", length);
				//Reading rest of package
				for (int i = 0; i < (length + 1); i++) {
					r = read(fd, readTemp, 1);
					if (r != 1) READ_ERROR = TRUE;
					packageRead[i + 3] = readTemp[0];
				}
			
				//testing for READ ERROR
				if (READ_ERROR == TRUE) printf("READ ERROR OCCURED IN THE PROGRAM \n");

							//calculating checksum
				char ChkSum = 0xFF;
				for (int i = 0; i < length; i++) {
					ChkSum -= packageRead[i + 3];
				}
				if (ChkSum != packageRead[(length + 3)]) printf("Checksum Error: Checksum do not match: \nCal: %x \nPac: %x\n\n", ChkSum, packageRead[length + 3]);
				else printf("Checksum: OK!\n");

							//printing package
				printf("Package [Hexa]: \n");
				for (int i = 0; i < length + 4; i++)
				{
					printf("%x ", packageRead[i]);
					fflush(stdout);
				}
				printf("\n");

							//Finding status from package:
				switch (packageRead[17])
				{
				case 0: 
					printf("Status: OK \n");
					break;
				case 1:
					printf("Status: ERROR \n");
					break;
				case 2:
					printf("Status: INVALID COMMAND \n");
					break;
				case 3:
					printf("Status: INVALID PARAMETER \n");
					break;
				case 4:
					printf("Status: TX FALURE \n");
					break;
				default:
					printf("STATUS ERROR, No sutch option \n");
					break;
				}
			}
		}
		printf("\n");
		READ_STOP = FALSE;
	
	}

	//Send AT Command:
	void send(char Start_delimitter, char mode, char *XBeeID, char *ATCommand, char *ATParameters, char ATParametersOn)
	{
		usleep(7000000);
		//mode = 0 => Remote Command API
		//mode = 1 => AT command

		int index = 0; //index of the package to be send
		packageSend[index] = Start_delimitter;
		index += 2; //for length bytes

			//Frame Type:
		index++;
		packageSend[index] = mode;
		//0x17 => Remote Command API
		//0x08 => AT Command
		//

			//Frame ID
		index++;
		packageSend[index] = 0x01; //ID set to zero = no response, 

			//64-bit ID added to package
		for (int i = 0; i < 8; i++)
		{
			index++; //one byte added to the package to be send
			packageSend[index] = XBeeID[i];
		}

			//16-bit address set to 0xFFFE
		index++;
		packageSend[index] = 0xFF;
		index++;
		packageSend[index] = 0xFE;

			//Remote cmd. Options: if not set, Apply Changes Command (AC), must be sent
			//bit 0: Disable ACK, bit 1: Apply Changes, bit 6, extended timeout 
		index++;
		packageSend[index] = 0b00000010; // = 0x02

			//AT Command
		index++;
		packageSend[index] = ATCommand[0];
		index++;
		packageSend[index] = ATCommand[1];

		char Command = 0;
		if ((ATCommand[0] == 'D') && (ATCommand[1] == '0' || ATCommand[1] == '1' || ATCommand[1] == '2' || ATCommand[1] == '3' || ATCommand[1] == '4' || ATCommand[1] == '5' || ATCommand[1] == '6' || ATCommand[1] == '7' || ATCommand[1] == '8')) Command = 1;
		if ((ATCommand[0] == 'P') && (ATCommand[1] == '0' || ATCommand[1] == '1' || ATCommand[1] == '2' || ATCommand[1] == '3')) Command = 2;
		if (ATCommand[0] == 'I' && ATCommand[1] == 'R') Command = 3;
		if (ATCommand[0] == 'I' && ATCommand[1] == 'C') Command = 4;
		if (ATCommand[0] == 'I' && ATCommand[1] == 'S') Command = 5;
		if (ATCommand[0] == '%' && ATCommand[1] == 'V') Command = 6;

			//Parameter Value
		if (ATParametersOn == 1)
		{
			switch (Command)
			{
			case 1:
				index++;
				packageSend[index] = ATParameters[0];
				break;
			case 2:
				index++;
				packageSend[index] = ATParameters[0];
				break;
			case 3:
				index++;
				packageSend[index] = ATParameters[0];
				index++;
				packageSend[index] = ATParameters[1];
				break;
			case 4:
				index++;
				packageSend[index] = ATParameters[0];
				index++;
				packageSend[index] = ATParameters[1];
				break;
			case 5:
				break;
			case 6:
				break;
			default:
				break; //error
			}
		}

			//there is 3 std. bytes always: 1 start, 2 length (chksum not yet added) and index starts from 0 therefor +1.
		packageSend[1] = (index - 3 + 1) >> 8;
		packageSend[2] = index - 3 + 1;

			//Checksum
		int length = (packageSend[1] << 8) + packageSend[2];
		index++;
		//calculating checksum
		packageSend[index] = 0xFF;
		for (int i = 0; i < length; i++) {
			packageSend[index] -= packageSend[i + 3];
		}



			//Printing Package:
		printf("Package to be send: \n");
		for (int i = 0; i < index + 1; i++)
		{
			printf("%x ", packageSend[i]);
		}
		printf("\n");

			//Sending Package
		int w = write(fd, packageSend, index + 1);
		printf("Bytes send: %d\n", w);
	}


















	//// Gammel Kode:


		//while (STOP == FALSE) { /* loop for input */
		//	int w = write(fd, packageOn, 20);
		//	printf("%d\n", w);
		//	usleep(1000);
		//	STOP = TRUE;
		//}


			//while (STOP == FALSE)
			//{
			//	par[0] = 0x05; // on
			//	send(0x7E, 0, XBeeID_LED, "D8", par);

				//	usleep(10000000);

					//	par[0] = 0x04; // off
					//	send(0x7E, 0, XBeeID_LED, "D8", par);
					//}

					//// 

						////Setting DIO0 as Analog Input Output
						//par[0] = 0x00; //Selection of DIO0
						//par[1] = 0x02;
						//send(0x7E, 0x17, XBeeID_LED, "IC", par, 1);
						////reciving Answer
						//receive();

							////Setting SampleRate
							//par[0] = 0x64; //Samplerate
							//send(0x7E, 0x17, XBeeID_LED, "IR", par, 1);
							////reciving Answer
							//receive();

								////Setting D1 to ADC
								//par[0] = 0x02; // Set as ADC[0x02]
								//send(0x7E, 0x17, XBeeID_LED, "D1", par, 1);
								////reciving Answer
								//receive();


									//char packageOn[50] = { 0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 0x41, 0x6B, 0x89, 0x49, 0xFF, 0xFE, 0x02, 0x44, 0x38, 0x05, 0x34 };
									//char packageOff[50] = { 0x7E, 0x00, 0x10, 0x17, 0x01, 0x00, 0x13, 0xA2, 0x00, 0x41, 0x6B, 0x89, 0x49, 0xFF, 0xFE, 0x02, 0x44, 0x38, 0x04, 0x35 };
