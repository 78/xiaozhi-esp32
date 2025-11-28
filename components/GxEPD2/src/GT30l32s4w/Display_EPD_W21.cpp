#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"

//Delay Functions
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static const char *TAG = "EPD_DEMO";
void delay_xms(unsigned int xms)
{
   vTaskDelay(xms / portTICK_PERIOD_MS);
}

////////////////////////////////////E-paper demo//////////////////////////////////////////////////////////
//Busy function
void Epaper_READBUSY(void)
{ 
	vTaskDelay(pdMS_TO_TICKS(10));
  while(1)
  {	 //=1 BUSY
	int ret = isEPD_W21_BUSY;
     if(ret==0) 
	 {
		
		ESP_LOGW(TAG, "IDLE: %d", ret);
		// vTaskDelay(pdMS_TO_TICKS(1000));
		break;
	}
	 else
	 {  
		ESP_LOGW(TAG, "BUSY: %d", ret);
	
	}
  }  
}
//Full screen refresh initialization
void EPD_HW_Init(void)
{
	delay_xms(10);//At least 10ms delay 
	//int ret = isEPD_W21_BUSY;
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	//vTaskDelay(pdMS_TO_TICKS(1000));
	//ESP_LOGW(TAG, "RST=0: %d", ret);

	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	//vTaskDelay(pdMS_TO_TICKS(1000));
	//ESP_LOGW(TAG, "RST=1: %d", ret);

	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	//delay_xms(10); //At least 10ms delay 
	Epaper_READBUSY();   
		
	EPD_W21_WriteCMD(0x01); //Driver output control      
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);   
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);

	EPD_W21_WriteCMD(0x21); //  Display update control
	EPD_W21_WriteDATA(0x40);		
  EPD_W21_WriteDATA(0x00);		

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);	
	
	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);   
	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);   
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
    Epaper_READBUSY();
	//delay_xms(10); //At least 10ms delay 
}
//Fast refresh 1 initialization
void EPD_HW_Init_Fast(void)
{
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
  
	EPD_W21_WriteCMD(0x12);  //SWRESET
	//delay_xms(10); //At least 10ms delay 
	Epaper_READBUSY();   
 
	EPD_W21_WriteCMD(0x21);	 
	EPD_W21_WriteDATA(0x40);
	EPD_W21_WriteDATA(0x00); 
	
	EPD_W21_WriteCMD(0x3C);     
	EPD_W21_WriteDATA(0x05);  

	//1.5s
	EPD_W21_WriteCMD(0x1A); // Write to temperature register
	EPD_W21_WriteDATA(0x6E);		  
				  	
	EPD_W21_WriteCMD(0x22); // Load temperature value
	EPD_W21_WriteDATA(0x91);		
  EPD_W21_WriteCMD(0x20);	
	Epaper_READBUSY();   
	
	EPD_W21_WriteCMD(0x11);  // Data entry mode
	EPD_W21_WriteDATA(0x01);   
	
	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    

	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 
	
	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
  Epaper_READBUSY();  

}


//////////////////////////////Display Update Function///////////////////////////////////////////////////////
//Full screen refresh update function
void EPD_Update(void)
{   
  EPD_W21_WriteCMD(0x22); //Display Update Control
  EPD_W21_WriteDATA(0xF7);   
  EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
  Epaper_READBUSY();   

}
//Fast refresh 1 update function
void EPD_Update_Fast(void)
{   
  EPD_W21_WriteCMD(0x22); //Display Update Control
  EPD_W21_WriteDATA(0xC7);   
  EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
  Epaper_READBUSY();   

}
//4 Gray refresh update function
void EPD_Update_4G(void)
{   
  EPD_W21_WriteCMD(0x22); //Display Update Control
  EPD_W21_WriteDATA(0xCF);   
  EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
  Epaper_READBUSY();   

}
//Partial refresh update function
void EPD_Part_Update(void)
{
	EPD_W21_WriteCMD(0x22); //Display Update Control
	EPD_W21_WriteDATA(0xFF);   
	EPD_W21_WriteCMD(0x20); //Activate Display Update Sequence
	Epaper_READBUSY(); 			
}
//////////////////////////////Display Data Transfer Function////////////////////////////////////////////
//Full screen refresh display function
void EPD_WhiteScreen_ALL(const unsigned char *datas)
{
   unsigned int i;	
  EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
  EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }	 
   EPD_Update();	 
}
//Fast refresh display function
void EPD_WhiteScreen_ALL_Fast(const unsigned char *datas)
{
   unsigned int i;	
  EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
   for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   } 
  EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
   for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   } 
	 	 
   EPD_Update_Fast();	 
}

//Clear screen display
void EPD_WhiteScreen_White(void)
{
 unsigned int i;
 EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0xff);
		// if ((i % 1000) == 0) 
		// {
		// 	 vTaskDelay(1 / portTICK_PERIOD_MS);
		// 	 taskYIELD();
		// }
			//ESP_LOGW(TAG, "I: %d", i);
	}
	
 EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0xff);
		// 		if ((i % 1000) == 0) 
		// {
		// 	 vTaskDelay(1 / portTICK_PERIOD_MS);
		// 	 taskYIELD();
		// }
				//ESP_LOGW(TAG, "J: %d", i);
	}

	EPD_Update();
}
//Display all black
void EPD_WhiteScreen_Black(void)
{
 unsigned int i;
 EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0x00);
	}
 EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
 for(i=0;i<EPD_ARRAY;i++)
 {
		EPD_W21_WriteDATA(0x00);
	}
	EPD_Update();
}
//Partial refresh of background display, this function is necessary, please do not delete it!!!
void EPD_SetRAMValue_BaseMap( const unsigned char * datas)
{
	unsigned int i;   	
  EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
  EPD_W21_WriteCMD(0x26);   //Write Black and White image to RAM
  for(i=0;i<EPD_ARRAY;i++)
   {               
     EPD_W21_WriteDATA(datas[i]);
   }
   EPD_Update();		 
	 
}
//Partial refresh display
void EPD_Dis_Part(unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
	unsigned int i;  
	unsigned int x_end,y_end;
	
	x_start=x_start/8; //x address start
	x_end=x_start+PART_LINE/8-1; //x address end
	y_start=y_start; //Y address start
	y_end=y_start+PART_COLUMN-1; //Y address end
	
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);	

	EPD_W21_WriteCMD(0x21); 
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
	
	EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
	EPD_W21_WriteDATA(x_start);  //x address start
	EPD_W21_WriteDATA(x_end);   //y address end   
	EPD_W21_WriteCMD(0x45);    // set RAM y address start/end
	EPD_W21_WriteDATA(y_start%256);  //y address start2 
	EPD_W21_WriteDATA(y_start/256); //y address start1 
	EPD_W21_WriteDATA(y_end%256);  //y address end2 
	EPD_W21_WriteDATA(y_end/256); //y address end1   

	EPD_W21_WriteCMD(0x4E);        // set RAM x address count to 0;
	EPD_W21_WriteDATA(x_start);   //x start address
	EPD_W21_WriteCMD(0x4F);      // set RAM y address count to 0X127;    
	EPD_W21_WriteDATA(y_start%256);//y address start2
	EPD_W21_WriteDATA(y_start/256);//y address start1
	
	
	 EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
   for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
   {                         
     EPD_W21_WriteDATA(datas[i]);
   } 
	 EPD_Part_Update();

}
//Full screen partial refresh display
void EPD_Dis_PartAll(const unsigned char * datas)
{
	unsigned int i;  
	unsigned int PART_COLUMN, PART_LINE;
	PART_COLUMN=EPD_HEIGHT,PART_LINE=EPD_WIDTH;

	EPD_W21_RST_0;  // Module reset   
	delay_xms(10); //At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);	

	EPD_W21_WriteCMD(0x21); 
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
	
	EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
	 for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
	 {                         
		 EPD_W21_WriteDATA(datas[i]);
	 } 
	 EPD_Part_Update();

}
//Deep sleep function
void EPD_DeepSleep(void)
{  	
  EPD_W21_WriteCMD(0x10); //Enter deep sleep
  EPD_W21_WriteDATA(0x01); 
  delay_xms(100);
}

//Partial refresh write address and data
void EPD_Dis_Part_RAM(unsigned int x_start,unsigned int y_start,const unsigned char * datas,unsigned int PART_COLUMN,unsigned int PART_LINE)
{
	unsigned int i;  
	unsigned int x_end,y_end;
	
	x_start=x_start/8; //x address start
	x_end=x_start+PART_LINE/8-1; //x address end
	
	y_start=y_start-1; //Y address start
	y_end=y_start+PART_COLUMN-1; //Y address end
	
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 	
	
	EPD_W21_WriteCMD(0x21); 
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom,
	EPD_W21_WriteDATA(0x80);	
	
	EPD_W21_WriteCMD(0x44);       // set RAM x address start/end
	EPD_W21_WriteDATA(x_start);  //x address start
	EPD_W21_WriteDATA(x_end);   //y address end   
	EPD_W21_WriteCMD(0x45);     // set RAM y address start/end
	EPD_W21_WriteDATA(y_start%256);  //y address start2 
	EPD_W21_WriteDATA(y_start/256); //y address start1 
	EPD_W21_WriteDATA(y_end%256);  //y address end2 
	EPD_W21_WriteDATA(y_end/256); //y address end1   

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(x_start);   //x start address
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X127;    
	EPD_W21_WriteDATA(y_start%256); //y address start2
	EPD_W21_WriteDATA(y_start/256); //y address start1
		
	EPD_W21_WriteCMD(0x24);   //Write Black and White image to RAM
  for(i=0;i<PART_COLUMN*PART_LINE/8;i++)
   {                         
     EPD_W21_WriteDATA(datas[i]);
   } 
}
//Clock display
void EPD_Dis_Part_Time(unsigned int x_startA,unsigned int y_startA,const unsigned char * datasA,
	                       unsigned int x_startB,unsigned int y_startB,const unsigned char * datasB,
												 unsigned int x_startC,unsigned int y_startC,const unsigned char * datasC,
												 unsigned int x_startD,unsigned int y_startD,const unsigned char * datasD,
											   unsigned int x_startE,unsigned int y_startE,const unsigned char * datasE,
												 unsigned int PART_COLUMN,unsigned int PART_LINE
	                      )
{
	EPD_Dis_Part_RAM(x_startA,y_startA,datasA,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startB,y_startB,datasB,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startC,y_startC,datasC,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startD,y_startD,datasD,PART_COLUMN,PART_LINE);
	EPD_Dis_Part_RAM(x_startE,y_startE,datasE,PART_COLUMN,PART_LINE);
	EPD_Part_Update();
}												 




////////////////////////////////Other newly added functions////////////////////////////////////////////
//Display rotation 180 degrees initialization
void EPD_HW_Init_180(void)
{
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10); //At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	
	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	Epaper_READBUSY();   
		
	EPD_W21_WriteCMD(0x21); //  Display update control
	EPD_W21_WriteDATA(0x40);		
  EPD_W21_WriteDATA(0x00);		
	
	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);	
	
	
	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x02);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    
	EPD_W21_WriteDATA(0x00);  

	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 
  EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);  
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00);
  Epaper_READBUSY();
}
// GUI initialization
void EPD_HW_Init_GUI(void)
{
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
	
	Epaper_READBUSY();   
	EPD_W21_WriteCMD(0x12);  //SWRESET
	Epaper_READBUSY();   
		
	EPD_W21_WriteCMD(0x01); //Driver output control      
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x01); //mirror

	EPD_W21_WriteCMD(0x21); //  Display update control
	EPD_W21_WriteDATA(0x40);		
  EPD_W21_WriteDATA(0x00);		

	EPD_W21_WriteCMD(0x3C); //BorderWavefrom
	EPD_W21_WriteDATA(0x05);	
	
	EPD_W21_WriteCMD(0x11); //data entry mode       
	EPD_W21_WriteDATA(0x01);

	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);   
	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);   
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 

	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
  Epaper_READBUSY();
	
}

//GUI display
void EPD_Display(unsigned char *Image)
{
	unsigned int Width, Height,i,j;
	Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
	Height = EPD_HEIGHT;

	EPD_W21_WriteCMD(0x24);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	}
	EPD_W21_WriteCMD(0x26);
	for ( j = 0; j < Height; j++) {
			for ( i = 0; i < Width; i++) {
				 EPD_W21_WriteDATA(Image[i + j * Width]);
			}
	 EPD_Update();
	}			
}
//4 Gray refresh  initialization
void EPD_HW_Init_4G(void)
{
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_0;  // Module reset   
	delay_xms(10);//At least 10ms delay 
	EPD_W21_RST_1;
	delay_xms(10); //At least 10ms delay 
  
	EPD_W21_WriteCMD(0x12);  //SWRESET
	//delay_xms(10); //At least 10ms delay 	
	Epaper_READBUSY();   
	
	EPD_W21_WriteCMD(0x3C);     
	EPD_W21_WriteDATA(0x05);  

	//4 Gray
	EPD_W21_WriteCMD(0x1A); // Write to temperature register
	EPD_W21_WriteDATA(0x5A);		  
				  	
	EPD_W21_WriteCMD(0x22); // Load temperature value
	EPD_W21_WriteDATA(0x91);		
  EPD_W21_WriteCMD(0x20);	
	Epaper_READBUSY();   
	
	EPD_W21_WriteCMD(0x11);  // Data entry mode
	EPD_W21_WriteDATA(0x01);   
	
	EPD_W21_WriteCMD(0x44); //set Ram-X address start/end position   
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(EPD_WIDTH/8-1);    
	EPD_W21_WriteCMD(0x45); //set Ram-Y address start/end position          
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteDATA(0x00); 
	
	EPD_W21_WriteCMD(0x4E);   // set RAM x address count to 0;
	EPD_W21_WriteDATA(0x00);
	EPD_W21_WriteCMD(0x4F);   // set RAM y address count to 0X199;    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)%256);    
	EPD_W21_WriteDATA((EPD_HEIGHT-1)/256);
  Epaper_READBUSY();     
}
unsigned char R24_DTM1(unsigned char Image_data1,unsigned char Image_data2)
{
	unsigned int i,all; 
	unsigned char TempData1,TempData2;
	unsigned char outdata=0x00;

		TempData1=Image_data1;
		TempData2=Image_data2;
		
		for(i=0;i<4;i++)
		{ 
			outdata=outdata<<1;
			if( ((TempData1&0xC0)==0xC0) || ((TempData1&0xC0)==0x40))
				outdata=outdata|0x01;
			else 
				outdata=outdata|0x00;

			TempData1=TempData1<<2;
 
		}

		for(all=0;all<4;all++)
		{ 
			outdata=outdata<<1;
			if((TempData2&0xC0)==0xC0||(TempData2&0xC0)==0x40)
				outdata=outdata|0x01;
			else 
				outdata=outdata|0x00;

			TempData2=TempData2<<2;

		}
		return outdata;
}
unsigned char R26_DTM2(unsigned char Image_data1,unsigned char Image_data2)
{
	unsigned int i,all; 
	unsigned char TempData1,TempData2;
	unsigned char outdata=0x00;
	TempData1=Image_data1;
	TempData2=Image_data2;
		
		for(i=0;i<4;i++)
		{ 
			outdata=outdata<<1;
			if( ((TempData1&0xC0)==0xC0) || ((TempData1&0xC0)==0x80))
				outdata=outdata|0x01;
			else 
				outdata=outdata|0x00;

			TempData1=TempData1<<2;
		}

		for(all=0;all<4;all++)
		{ 
			outdata=outdata<<1;
			if((TempData2&0xC0)==0xC0||(TempData2&0xC0)==0x80)
				outdata=outdata|0x01;
			else 
				outdata=outdata|0x00;

			TempData2=TempData2<<2;

		}
		return outdata;
}
//4 Gray refresh display function
void EPD_WhiteScreen_ALL_4G(const unsigned char *datas)
{
  unsigned int i; 
  unsigned char value;   
  EPD_W21_WriteCMD(0x24);   //write RAM for black(0)/white (1)           
	for(i=0;i<EPD_ARRAY*2;i+=2)
	{   
		value= R24_DTM1( *(datas+i),*(datas+i+1));
		EPD_W21_WriteDATA(value); 
	} 
  EPD_W21_WriteCMD(0x26);   //write RAM for black(0)/white (1)
	for(i=0;i<EPD_ARRAY*2;i+=2)
	{   
		value= R26_DTM2( *(datas+i),*(datas+i+1));
		EPD_W21_WriteDATA(value); 
	} 
	 
   EPD_Update_4G();	 
}

void pic_display_4line(void)
{
  unsigned int row,col;
	EPD_W21_WriteCMD(0x24);			//¿ªÊ¼´«ÊäºÚ°×Í¼Ïñ	                             
  for(col=0;col<300;col++)  
    { 
			for(row=0;row<13;row++)
			{ 
				EPD_W21_WriteDATA(0xFF);
			} 
			for(row=0;row<12;row++)
			{ 
				EPD_W21_WriteDATA(0x00);
			} 
			for(row=0;row<12;row++)
			{ 
				EPD_W21_WriteDATA(0xFF);
			} 
			for(row=0;row<13;row++)
			{ 
				EPD_W21_WriteDATA(0x00);
			}
		}
	
	  EPD_W21_WriteCMD(0x26);			//¿ªÊ¼´«ÊäºìÍ¼Ïñ	                       
		for(col=0;col<300;col++)
    { 
			for(row=0;row<13;row++)
			{ 
				EPD_W21_WriteDATA(0xFF);
			} 
			for(row=0;row<12;row++)
			{ 
				EPD_W21_WriteDATA(0xFF);
			} 
			for(row=0;row<12;row++)
			{ 
				EPD_W21_WriteDATA(0x00);
			} 
			for(row=0;row<13;row++)
			{ 
				EPD_W21_WriteDATA(0x00);
			}
		}	
	
	EPD_Update_4G();	 
}

/***********************************************************
						end file
***********************************************************/

