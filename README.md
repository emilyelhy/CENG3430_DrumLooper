# CENG3430_DrumLooper
### 20/05
*Use 2016 latest ver in github   
*change board_part when 1st use create_project.tcl   
*upgrade ip when 1st use create_project.tcl   
*add ff.h that library in checklist and property->linker->library to read sd card   
*change the .xdc file to include the 5 btn   
*BTNR<->BTND, BTNU<->BTNC [try to fix this tmr in .xdc file]   
*Use HPH out as output [prefer]   
*2 cables are required [1for UART, 1for PROG]   
*use tera for serial in/out   
*prefer WAV file for reading in SD card   

### 21/05
*Add "u32 SectorCount;" in line194 in xsdps.h in dma_audio_bsp   
*Correct the library path in line89 in diskio.c in dma_audio_bsp   
