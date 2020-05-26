/************************************************************************/
/*																		*/
/*	demo.c	--	Zedboard DMA Demo				 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Lowe											*/
/*	Copyright 2015, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		DMA audio inputs and outputs on the Zedboard.					*/
/*																		*/
/*																		*/
/************************************************************************/
/*  Notes:																*/
/*																		*/
/*		- The DMA max burst size needs to be set to 16 or less			*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		8/23/2016(SamL): Created										*/
/*																		*/
/************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xil_printf.h"
#include "demo.h"




#include "audio/audio.h"
#include "dma/dma.h"
#include "intc/intc.h"
#include "userio/userio.h"
#include "iic/iic.h"
#include "ff.h"

/***************************** Include Files *********************************/

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "xiic.h"
#include "xaxidma.h"



#ifdef XPAR_INTC_0_DEVICE_ID
 #include "xintc.h"
 #include "microblaze_sleep.h"
#else
 #include "xscugic.h"
#include "sleep.h"
#include "xil_cache.h"
#endif

/************************** Constant Definitions *****************************/

/*
 * Device hardware build related constants.
 */

// Audio constants
// Number of seconds to record/playback
#define NR_SEC_TO_REC_PLAY		5

// ADC/DAC sampling rate in Hz
//#define AUDIO_SAMPLING_RATE		1000
#define AUDIO_SAMPLING_RATE	  96000

// Number of samples to record/playback
#define NR_AUDIO_SAMPLES		(NR_SEC_TO_REC_PLAY*AUDIO_SAMPLING_RATE)

/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000

#define TEST_START_VALUE	0x0


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/
#if (!defined(DEBUG))
extern void xil_printf(const char *format, ...);
#endif


/************************** Variable Definitions *****************************/
/*
 * Device instance definitions
 */

static XIic sIic;
static XAxiDma sAxiDma;		/* Instance of the XAxiDma */

static XGpio sUserIO;

#ifdef XPAR_INTC_0_DEVICE_ID
 static XIntc sIntc;
#else
 static XScuGic sIntc;
#endif

//
// Interrupt vector table
#ifdef XPAR_INTC_0_DEVICE_ID
const ivt_t ivt[] = {
	//IIC
	{XPAR_AXI_INTC_0_AXI_IIC_0_IIC2INTC_IRPT_INTR, (XInterruptHandler)XIic_InterruptHandler, &sIic},
	//DMA Stream to MemoryMap Interrupt handler
	{XPAR_AXI_INTC_0_AXI_DMA_0_S2MM_INTROUT_INTR, (XInterruptHandler)fnS2MMInterruptHandler, &sAxiDma},
	//DMA MemoryMap to Stream Interrupt handler
	{XPAR_AXI_INTC_0_AXI_DMA_0_MM2S_INTROUT_INTR, (XInterruptHandler)fnMM2SInterruptHandler, &sAxiDma},
	//User I/O (buttons, switches, LEDs)
	{XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR, (XInterruptHandler)fnUserIOIsr, &sUserIO}
};
#else
const ivt_t ivt[] = {
	//IIC
	{XPAR_FABRIC_AXI_IIC_0_IIC2INTC_IRPT_INTR, (Xil_ExceptionHandler)XIic_InterruptHandler, &sIic},
	//DMA Stream to MemoryMap Interrupt handler
	{XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR, (Xil_ExceptionHandler)fnS2MMInterruptHandler, &sAxiDma},
	//DMA MemoryMap to Stream Interrupt handler
	{XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR, (Xil_ExceptionHandler)fnMM2SInterruptHandler, &sAxiDma},
	//User I/O (buttons, switches, LEDs)
	{XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR, (Xil_ExceptionHandler)fnUserIOIsr, &sUserIO}
};
#endif


/*****************************************************************************/
/**
*
* Main function
*
* This function is the main entry of the interrupt test. It does the following:
*	Initialize the interrupt controller
*	Initialize the IIC controller
*	Initialize the User I/O driver
*	Initialize the DMA engine
*	Initialize the Audio I2S controller
*	Enable the interrupts
*	Wait for a button event then start selected task
*	Wait for task to complete
*
* @param	None
*
* @return
*		- XST_SUCCESS if example finishes successfully
*		- XST_FAILURE if example fails.
*
* @note		None.
*
******************************************************************************/
int main(void)
{
	int playFlag = 0;
	int track = 1;
	int Status;

	Demo.u8Verbose = 1;

	//Xil_DCacheDisable();

	xil_printf("\r\n--- Entering main() --- \r\n");


	//
	//Initialize the interrupt controller

	Status = fnInitInterruptController(&sIntc);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing interrupts");
		return XST_FAILURE;
	}


	// Initialize IIC controller
	Status = fnInitIic(&sIic);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing I2C controller");
		return XST_FAILURE;
	}

    // Initialize User I/O driver
    Status = fnInitUserIO(&sUserIO);
    if(Status != XST_SUCCESS) {
    	xil_printf("User I/O ERROR");
    	return XST_FAILURE;
    }


	//Initialize DMA
	Status = fnConfigDma(&sAxiDma);
	if(Status != XST_SUCCESS) {
		xil_printf("DMA configuration ERROR");
		return XST_FAILURE;
	}


	//Initialize Audio I2S
	Status = fnInitAudio();
	if(Status != XST_SUCCESS) {
		xil_printf("Audio initializing ERROR");
		return XST_FAILURE;
	}


	// Enable all interrupts in our interrupt vector table
	// Make sure all driver instances using interrupts are initialized first
	fnEnableInterrupts(&sIntc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));



    xil_printf("DONE\r\n");

    xil_printf("-------------Start SD Card Init-------------\r\n");
    FATFS FS_instance;
    FRESULT result = f_mount(&FS_instance,"0:/", 1);
	if (result != 0) {
		xil_printf("[ERROR] Couldn't mount SD Card.\r\n");
		return XST_FAILURE;
	}
	xil_printf("[SUCCESS] Mounted SD Card\r\n");
	DIR dir;
	result = f_opendir(&dir, "0:/");
	if (result != FR_OK) {
		xil_printf("[ERROR] Couldn't read root directory.\r\n");
		return XST_FAILURE;
	}
	xil_printf("[SUCCESS] Read Root Directory\r\n");
	for(int i = 1; i < 9; i++){
		int flag = 0;
		char filename[6];
		filename[0] = i + 48;
		filename[1] = '.';
		filename[2] = 'W';
		filename[3] = 'A';
		filename[4] = 'V';
		filename[5] = '\0';
		xil_printf("Finding %s\r\n",filename);
		while(1){
			FILINFO fno;
			result = f_readdir(&dir, &fno);
			if (result != FR_OK || fno.fname[0] == 0) break;
//			xil_printf("Current FILE: %s\r\n",fno.fname);
			int temp = 0;
			for(int j = 0; fno.fname[j] != 0 && filename[j] != 0; j++){
//				xil_printf("fno.fname[%d]: %c  file[%d]: %c\r\n",j,fno.fname[j],j, filename[j]);
				if(fno.fname[j] != filename[j]) temp = 1;
			}
			if(temp == 0){
				xil_printf("[SUCCESS] Found %s\r\n",filename);
				flag = 1;
				break;
			}
		}
		if(flag == 0){
			xil_printf("[ERROR] %s not found\r\n",filename);
			return XST_FAILURE;
		}
	}
	f_closedir(&dir);
	xil_printf("------------SD Card Init SUCCESS------------\r\n");

    //main loop

    while(1) {


//    	xil_printf("----------------------------------------------------------\r\n");
//		xil_printf("Genesys 2 DMA Audio Demo\r\n");
//		xil_printf("----------------------------------------------------------\r\n");

    	//Xil_DCacheDisable();

    	// Checking the DMA S2MM event flag
    			if (Demo.fDmaS2MMEvent)
    			{
    				xil_printf("\r\nRecording Done...");

    				// Disable Stream function to send data (S2MM)
    				Xil_Out32(I2S_STREAM_CONTROL_REG, 0x00000000);
    				Xil_Out32(I2S_TRANSFER_CONTROL_REG, 0x00000000);
    				//Flush cache
    				//Flush cache


//					//microblaze_flush_dcache();
					//Xil_DCacheInvalidateRange((u32) MEM_BASE_ADDR, 5*NR_AUDIO_SAMPLES);
    				//microblaze_invalidate_dcache();
    				// Reset S2MM event and record flag
    				Demo.fDmaS2MMEvent = 0;
    				Demo.fAudioRecord = 0;
    			}

    			// Checking the DMA MM2S event flag
    			if (Demo.fDmaMM2SEvent)
    			{
    				xil_printf("\r\nPlayback Done...");

    				// Disable Stream function to send data (S2MM)
    				Xil_Out32(I2S_STREAM_CONTROL_REG, 0x00000000);
    				Xil_Out32(I2S_TRANSFER_CONTROL_REG, 0x00000000);
    				//Flush cache
//					//microblaze_flush_dcache();
    				//Xil_DCacheFlushRange((u32) MEM_BASE_ADDR, 5*NR_AUDIO_SAMPLES);
    				// Reset MM2S event and playback flag
    				Demo.fDmaMM2SEvent = 0;
    				Demo.fAudioPlayback = 0;
    			}

    			// Checking the DMA Error event flag
    			if (Demo.fDmaError)
    			{
    				xil_printf("\r\nDma Error...");
    				xil_printf("\r\nDma Reset...");


    				Demo.fDmaError = 0;
    				Demo.fAudioPlayback = 0;
    				Demo.fAudioRecord = 0;
    			}

    			// Checking the btn change event
    			if(Demo.fUserIOEvent) {

    				switch(Demo.chBtn) {
    					case 'u':
    						break;
    					case 'd':
    						break;
    					case 'r':
    						if(track < 8) track = track + 1;
    						xil_printf("\r\nTrack: %d", track);
    						break;
    					case 'l':
    						if(track > 1) track = track - 1;
    						xil_printf("\r\nTrack: %d", track);
    						break;
    					case 'c': ;
    						char filename[6];
    						filename[0] = track + 48;
    						filename[1] = '.';
    						filename[2] = 'w';
    						filename[3] = 'a';
    						filename[4] = 'v';
    						filename[5] = '\0';
    						FIL file;
    						FRESULT result = f_open(&file, filename, FA_READ);
    						if(result != 0){
    							xil_printf("[ERROR] Could not open %s\r\n",filename);
    							return XST_FAILURE;
    						}
    						xil_printf("\r\nPlaying: %s\r\n",filename);
    						playFlag = 1;
    						fnSetHpOutput();
    						int flag = playFileAtHp(filename, sAxiDma);
    						if(flag == XST_FAILURE) return XST_FAILURE;
    						break;
    					default:
    						break;
    				}

    				// Reset the user I/O flag
    				Demo.chBtn = 0;
    				Demo.fUserIOEvent = 0;


    			}
    	//usleep(90000);
    }

	xil_printf("\r\n--- Exiting main() --- \r\n");


	return XST_SUCCESS;

}









