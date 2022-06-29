#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

//interface.c is the main program to communicate with the fpga by accessing the register file. The transaction is:
//First, PC sends START_TRANSMISSION (specific address 0x01010101 --> check if I'm not trying to send the same) then the register file reply with START as ackn, then PC sends address through TX (GPIO1:0x4200_0000),
//the register file will reply with the ADDRESS to the RX port(GPIO2: 0x4200_0008). Once it's received the PC verifies that is the same sent and start sending the data to store in the corresponding register.  

//To do: start to organize everything as a function to send and receive that sends only the arguments 
int main(int argc, char **argv)
{
int fd, mode_sel, kp, ki, kd, mux_ch1, mux_ch2, PSR = 14, DSR =10, ISR=24, kpx, kix = 0, kdx , PSRx = 24, ISRx = 0, DSRx= 10, mux_fil_in = 0, mux_fil_out = 0;
float wait_time = 1.0, time_length=0.0, counter_cfg=0.0, min_val_analog=0.0, max_val_analog=0.0, lsb = 0.00012207, min_val_dig=0.0, max_val_dig=0.0, time_steps = 0.0, off_ch1, off_ch2, off_ch1_d, off_ch2_d, off_ch3, off_ch3_d;
long double kp_int, kph_int, kd_int, ki_int;
uint32_t debug_val1=0, debug_val2=0, debug_val3=0, debug_val4=0; 					// For all, bit [13:0] effective data, other are not important
uint32_t pid1addr = 1, pid2addr = 2, muxaddr = 3, offsetaddr = 4, dith1addr = 5, dith2addr = 6, pid_g_addr = 9, pid3addr = 10, pid4addr = 11, pid_g_1_addr = 12, dith3addr = 13, dith4addr = 14, off1addr = 15, fil1addr = 16,
	 fila0addr = 17,  fila1addr = 18, fila2addr = 19, filb1addr = 20, filb2addr = 21,   START = 16843009;
unsigned repeat = 1;
void *cfg;							
char *name = "/dev/mem";
const int freq = 124998750; // Hz

printf("------------------------------START-----------------------------\n");

if((fd = open(name, O_RDWR)) < 0) {
    perror("open");
    return 1;
}
cfg = mmap(NULL, sysconf(_SC_PAGESIZE), /* map the memory */
            PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x42000000);

printf("What parameter do you want to set?\n PID : 1, MUX_OUT: 2, OFFSET: 3, DITHER: 4, PID_GAIN: 5, PD_EXT_SET: 6, PD_EXT_GAIN: 7, OFFSET_PID_CH1: 8, FILTER_LIA_SET : 9\n");
scanf("%d", &mode_sel);

if (mode_sel == 1)
	{
	printf("PID CURRENT PARAMETERS:\n PSR = %d, ISR = %d, DSR = %d\n", PSR, ISR, DSR);
	printf("Kp=?\n");
	scanf("%d", &kp);
	printf("Ki=?\n");
	scanf("%d", &ki);
	printf("Kd=?\n");
	scanf("%d", &kd);

	/*Communication starts */

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	
	if(*((uint32_t *)(cfg + 8)) == START)
		{
			printf("Start transaction:\n");
			*((uint32_t *)(cfg + 0)) = pid2addr;
		}

	if (*((uint32_t *)(cfg + 8)) == pid2addr) 		//Address received correctly
		{
			*((uint32_t *)(cfg + 0)) = 0;	//Reset everything
			//sleep(wait_time);
			printf("RESET = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	
	if(*((uint32_t *)(cfg + 8)) == START)
		{
			printf("Start transaction:\n");
			*((uint32_t *)(cfg + 0)) = pid1addr;
		}
	if (*((uint32_t *)(cfg + 8)) == pid1addr) 		//Address received correctly
		{
			*((uint32_t *)(cfg + 0)) = ((uint16_t)ki << 16) + (uint16_t)kp;	//FIRST WRITE
			printf("FIRST_WRITE = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	
	if(*((uint32_t *)(cfg + 8)) == START)
		{
			printf("Start transaction:\n");
			*((uint32_t *)(cfg + 0)) = pid2addr;
		}
	if (*((uint32_t *)(cfg + 8)) == pid2addr) 		//Address received correctly
		{
			if (kd < 0) 
				*((uint32_t *)(cfg + 0)) = (uint16_t)kd;
			else 
				*((uint32_t *)(cfg + 0)) = (3 << 30) + (uint16_t)kd; 
			printf("SECOND_WRITE = CORRECT\n");

		}
	
	kp_int = kp >> 14;
	kph_int = kp >> 12;
	ki_int = 0.000000008/(ki >> 24);
	kd_int = 0.000000008/(kd >> 10);

	printf("---------------------------PID SETTING COMPLETE------------------------\n");
	printf("VALUES SET: Kp = %Lg, Kp(high gain) = %Lg,  taui = %Lg, taud = %Lg \n", kp_int, kph_int, ki_int, kd_int );
	printf("REG1: %#010x, REG2: %#010x\n", ((uint32_t)ki << 14) + (uint32_t)kp,  *((uint32_t *)(cfg + 0)) );
	}
else if(mode_sel == 2)			//MUX OUT CHANNEL SELECTION
	{
		printf("Select what do you want to display on CH1:\n PID_mixer = 1, INTEGRAL ERROR PID (debug) = 2, PID_CH1 = 3, PID_OFFSET_CH1 = 4, PID_mixer + notch	: 5, dither_out = 6, OFFSET_PID_MIXER = 7, ERROR_SIGNAL(AFTER_LPF) <(-)> = 8  \n");
		scanf("%d", &mux_ch1); 

		printf("Select what do you want to display on CH2:\n ERROR_SIGNAL(AFTER LPF) = 1, MIXER_EXT_DDS(No filter) = 2, cosine_normal = 3, cos_scaled = 4, pd_ext_mixed = 5, phase_normal = 6, CH2 IN to OUT (see noise given by RP) = 7, MIXER_INT_DDS(No Filter) = 8 \n");
		scanf("%d", &mux_ch2); 

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = muxaddr;
	//sleep(wait_time);
	}
	if (*((uint32_t *)(cfg + 8)) == muxaddr) 		//Address received correctly
		{ 
			*((uint32_t *)(cfg + 0)) = (uint32_t)((mux_ch2-1)<<3) + (uint32_t)(mux_ch1-1);
			printf("REG SENT = %#010x\n", *((uint32_t *)(cfg + 0)) );
			printf("CHANNEL_SELECT = CORRECT\n");
		}
	else printf("CHANNEL_SELECT = NOT CORRECT\n");

	}
else if(mode_sel == 3)		//OFFSET SELECTION PID_MIXER 
	{
	
	printf("Select the offset level for pid_mixer:\n");
	scanf("%g", &off_ch1);
	
	//printf("Select the offset level for pid_ext_dds:\n");
	//scanf("%g", &off_ch2);

	off_ch1_d = off_ch1 / lsb;
	//off_ch2_d = off_ch2 / lsb;

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = offsetaddr;
	sleep(wait_time);
	}
	if (*((uint32_t *)(cfg + 8)) == offsetaddr) 		//Address received correctly
		{ 
			//*((uint32_t *)(cfg + 0)) = ((int16_t)(off_ch2_d)<<16) + (int16_t)(off_ch1_d);   TO USE IN CASE OF A CH2 OFFSET
			*((uint32_t *)(cfg + 0)) = (int16_t)(off_ch1_d);
			printf("REG SENT = %#010x\n",	*((uint32_t *)(cfg + 0)) );
			printf("OFFSET_SELECT = CORRECT\n");
		}
	}

else if(mode_sel ==4)		//DITHER SET
	{	
	printf (" Set the length of the Dither, from 1 ms to 10 s (i.e. 0.125, 1.5, 2.25) \n");
	scanf ("%g", &time_length);

	printf (" Set the minimum value, minimum value is -1V (i.e. -0.850, 0.1) \n");
	scanf ("%g", &min_val_analog);

	printf (" Set the maximum value, maximum value has to be higher than %g and lower than +1 V (i.e. with a minimum value of 0.1, 0.25)\n", min_val_analog);
	scanf ("%g", &max_val_analog);

	if (max_val_analog <= min_val_analog)
		{
			printf("Max value that you've tried to set is lower than the minimum value. The value is set to +1V \n");
			max_val_analog = 1;
		}

	min_val_dig = min_val_analog / lsb;
	max_val_dig = max_val_analog / lsb;

	if (max_val_dig > 8191) 
		max_val_dig = 8191;
	if (min_val_dig < -8192) 
		min_val_dig = -8192;

	if (min_val_dig == 0)
		min_val_dig = 1;
	if (max_val_dig == 0)
		max_val_dig = 1; 

	
	time_steps=time_length * freq;

	counter_cfg = time_steps/(max_val_dig - min_val_dig);

	printf ("max: %g, min: %g, counter: %g", max_val_dig, min_val_dig, counter_cfg);

	debug_val1 = ((uint32_t)counter_cfg);
	debug_val2 = (int16_t)min_val_dig;
	debug_val3 = (int16_t)max_val_dig;	

	printf (" DIT1: %#010x, DIT2: %#010x , DIT3: %#010x \n", debug_val1, debug_val2, debug_val3);
	
	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready //RESET DITHER + GPIO to AXIS
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith4addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith4addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = (1 << 29);
				//sleep(wait_time);

			
			printf("DIT_RESET1 = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready //RESET DITHER + GPIO to AXIS
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith4addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith4addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = 0;
				//sleep(wait_time);

			
			printf("DIT_RESET = COMPLETE\n");
		}


	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith1addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith1addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = ((uint32_t)counter_cfg);
				//sleep(wait_time);

			
			printf("DITHER1 = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith2addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith2addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = (int16_t)min_val_dig;
				//sleep(wait_time);

			
			printf("DITHER2 = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith3addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith3addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = (int16_t)max_val_dig;
				//sleep(wait_time);

			
			printf("DITHER3 = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	//sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = dith4addr;
	//sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == dith4addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = (1 << 31) + (repeat << 30) + (rand()%100 + 1);
				//sleep(wait_time);

			printf("REG SENT = %#010x\n",	*((uint32_t *)(cfg + 0)) );

			printf("DITHER4 = CORRECT\n");
		}

	}
else if (mode_sel == 5)
	{
	printf("GAIN VALUES: PSR = %d, ISR = %d, DSR = %d\nEnter the new ones:\n", PSR, ISR, DSR);
	printf("ENTER PSR, accepted values 6,8,10,12,14,16:\n");
	scanf("%d", &PSR);
	printf("ENTER ISR, accepted values 16,18,20,22,24,26,28:\n");
	scanf("%d", &ISR);
	printf("ENTER DSR, accepted values 6,8,10,12:\n");
	scanf("%d", &DSR);

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = pid_g_addr;
	sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == pid_g_addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = ((uint32_t)DSR << 10)+ ((uint32_t)ISR << 5) + (uint32_t)PSR;
				sleep(wait_time);

			printf("REG SENT = %#010x\n",	*((uint32_t *)(cfg + 0)) );
			printf("SETPIDGAIN = CORRECT\n");
		}


	}
else if (mode_sel == 6)
	{
	printf("PD CURRENT PARAMETERS:\n PSR = 14, DSR = 10\n");
	printf("Kp=?\n");
	scanf("%d", &kpx);
	printf("Kd=?\n");
	scanf("%d", &kdx);

	/*Communication starts */

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = pid4addr;
	sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == pid4addr) 		//Address received correctly
		{
			*((uint32_t *)(cfg + 0)) = 0;	//Reset everything
			sleep(wait_time);
			printf("RESET = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = pid3addr;
	sleep(wait_time);
	}


	if (*((uint32_t *)(cfg + 8)) == pid3addr) 		//Address received correctly
		{
			*((uint32_t *)(cfg + 0)) = ((uint32_t)kix << 14) + (uint32_t)kpx;	//FIRST WRITE
			sleep(wait_time);
			printf("FIRST_WRITE = CORRECT\n");
		}

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = pid4addr;
	sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == pid4addr) 		//Address received correctly
		{
			if (kdx < 0) 
				*((uint32_t *)(cfg + 0)) = (uint32_t)kdx;
			else 
				*((uint32_t *)(cfg + 0)) = (3 << 30) + (uint32_t)kdx; 
			sleep(wait_time);
			printf("SECOND_WRITE = CORRECT\n");

		}
	}
else if (mode_sel == 7)
	{
	printf("DEFAULT GAIN VALUES: PSR = 14, DSR = 10\nEnter the new ones:\n");
	printf("ENTER PSR, accepted values 6,8,10,12,14,16:\n");
	scanf("%d", &PSRx);
	printf("ENTER DSR, accepted values 6,8,10,12:\n");
	scanf("%d", &DSRx);

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	sleep(wait_time);
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = pid_g_1_addr;
	sleep(wait_time);
	}

	if (*((uint32_t *)(cfg + 8)) == pid_g_1_addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) = ((uint32_t)DSRx << 10)+ ((uint32_t)ISRx << 5) + (uint32_t)PSRx;
				sleep(wait_time);

			printf("REG SENT = %#010x\n",	*((uint32_t *)(cfg + 0)) );
			printf("SETPDGAIN = CORRECT\n");
		}


	}
else if(mode_sel == 8)
	{
	
	printf("Select the offset level for pid_ch1:\n");
	scanf("%g", &off_ch3);

	off_ch3_d = off_ch3 / lsb;

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready
	
	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = off1addr;
	}


	if (*((uint32_t *)(cfg + 8)) == off1addr) 		//Address received correctly
		{ 
				*((uint32_t *)(cfg + 0)) =(uint16_t)(off_ch3_d);

			printf("REG SENT = %#010x\n",	*((uint32_t *)(cfg + 0)) );
			printf("OFFSET_1_SELECT = CORRECT\n");
		}

	
	
	}

else if(mode_sel == 9)
	{
	printf("Select the INPUT of the filter:\nmixer_internal_DDS : 1 (default); mixer_external_DDS : 2\n");
	scanf("%d", &mux_fil_in);
	printf("Select the filter at the output of the mixer module:\nIIR_1M_leaky : 1; IIR_600k_leaky : 2; IIR_153kHz : 3; IIR_notch : 4; // : 5;  // : 6; IIR_2nd_1M: 7 no_filter: 8   \n");
	scanf("%d", &mux_fil_out);

	printf("mux_fil_in = %d,mux_fil_out =%d, mux_fil_in uint32 = %zu, mux_fil_out uint32 = %zu\n", mux_fil_in, mux_fil_out, (uint32_t)(mux_fil_in), (uint32_t)(mux_fil_out-1));

	*((uint32_t *)(cfg + 0)) = START; //Switch the FSM to ready

	if(*((uint32_t *)(cfg + 8)) == START)
	{
	printf("Start transaction:\n");
	*((uint32_t *)(cfg + 0)) = fil1addr;
	printf("REG SENT = %#010x\n", *((uint32_t *)(cfg + 0)) );
	}
	printf("REG RECEIVED = %#010x\n", *((uint32_t *)(cfg + 0)) );
	if (*((uint32_t *)(cfg + 8)) == fil1addr) 		//Address received correctly
		{  
				*((uint32_t *)(cfg + 0)) = (uint32_t)((mux_fil_in-1)<<3) + (uint32_t)(mux_fil_out-1);

			printf("REG SENT = %#010x\n", *((uint32_t *)(cfg + 0)) );
			

			printf("FILTER_SELECT = CORRECT\n");
			
		}
	else printf("FILTER_SELECT = NOT CORRECT\n");

	}


munmap(cfg, sysconf(_SC_PAGESIZE));
return 0;
}

