/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2018 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* USER CODE BEGIN Includes */
#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include <string.h>

#include "melpe_old/melpe.h" //MELPE 800 bps speech codec
#include "mdm/modem.h" //800 bps Pulse and BPSK modems
#include "crp/cntr.h" //Control (key exchange, encrypting, statistic)


/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;
DMA_HandleTypeDef hdma_dac2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

#define SPEECH_FRAME 540 //short PCM samples in 90 mS speech frame
#define SPEECH_RATE 6000 //Mike and Speaker sampling rates, Hz

#define MODEM_FRAME 720 //short PCM samples in 90 mS modem frame
#define MODEM_RATE 8000 //Line input and output sampling rates, Hz


//PCM double buffers
int16_t inbuf[2*SPEECH_FRAME]; //recording from Mike
int16_t outbuf[2*SPEECH_FRAME]; //playing over Speaker
int16_t inmdm[2*MODEM_FRAME]; //recording from Line
int16_t outmdm[2*MODEM_FRAME]; //playing to Line

//data buffers
unsigned char bits[16]; //data block (9 data bytes + flags)
unsigned char sbts[72]; //soft bits of received data bytes (LLR)
signed char lbuf[256]={0}; //log output text buffer

//for TRNG
volatile unsigned int adcres; //result of ADC conversion for TRNG
int adccnt; //counters of ADC probes fro TRNG

//synchronization levels setted in interrupts
volatile unsigned int dac1_level=530; //level of voice playing DMA buffer in a moment of new baseband frame was recorded
volatile unsigned int dac2_level=710; //level of baseband playing DMA buffer in a moment of new voice frame was recorded
volatile unsigned short tim3period=11250; //divider for nominal samle rate 8000KHz of recording from Line (tuned by demodulator)

//ready flags setted in interupts
volatile unsigned char Start_Encoding = 0; //flag of voice frame was recorded
volatile unsigned char Start_Decoding=0; //flag of baseband frame was recorded

//UART TX log output
signed char* tx_ptr=lbuf; //pointer to char in log buffer will be transmitted
//status flags
signed char key=0; //flag of having their key (0-no key, 1-have key untill button will be presed, -1 -  have key)
unsigned char snc; //flag of their carrier detected (0- no carrier, 1-carrier detected and synchronized)
unsigned char btn; //flag of button pressed (0-button released, 1-pressed)
unsigned char vad; //flag of our speech active (0-silency, 1-speech)
unsigned char work=0;  //current device status: 0-IKE (after run), 1-WORK mode (after key agreeded)
unsigned char psk=0; //flag of using PSK modem instead pulse one
unsigned char ptt=0; //flag of push-to-talk mode (function of button will be inversed)

volatile unsigned int k;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void getJmp(void);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
//blue btn=PC13
//psk=PC8
//vad=PC6
//ptt=PA11
//LedR=PB9
//Ledy=PA7
 
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  int i;
	 
	int16_t* sp_ptr;
  int16_t* md_ptr;
	uint8_t ptr; //cashing of volatile Start_Encoding/Start_Decoding
	
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
//----------------------------------------------------------------------
//init melpe 800 bps codec at 6KHz sampling rate
  melpe_i();	

//----------------------------------------------------------------------
  //self test crypto engine by test vector
  if(!testcrp()) while(1){}; 

		//---------------------------------------------------------------------- 
  //collect entropy from Mike using one LSB only 
  while(1)
  {	 
   HAL_ADC_Stop(&hadc3);
	 HAL_ADC_Start(&hadc3); //restart ADC3 in continuous mode 
	 adccnt=100000; //timeout for restarting ADC3
	 while(--adccnt) //loop untill timeout elapsed
	 {
	  adcres=HAL_ADC_GetValue(&hadc3); //get current ADC value
	  i=setrand(adcres); //add LSB to entropy
	  if(i) break; //break if we have sufficient entropy
	 }
	 if(i) break; //break if we have sufficient entropy
  }
  HAL_ADC_Stop(&hadc3); //stop ADC3 
 
	//Scan today's secret code from jumpers
	//Note: this produce near 12 bit level. Designed for test board! 
	//Please replace this while migrate to real device!
//----------------------------------------------------------------------	
	  k=0; //clear code accumulator
	  //scan 8 bits jumper's code 0
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7)) k|=1; k<<=1;
    
	  HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1); //back group 2 level to 1
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);		
	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9); //change group 0 level to 0
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);		
	  for(i=0;i<10000;i++) adcres++; //delay 
	  
	  //scan 8 bits jumpers code 1
	 if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7)) k|=1; k<<=1;

    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_9); //back group0 level to 1
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10); //change group1 level to 0
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_10);
    for(i=0;i<10000;i++) adcres++; //delay
		
		//scan 8 bits jumper's code 2
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)) k|=1; k<<=1;
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_7)) k|=1;
		
    setkey(k); //setup new session keypair 
    k=0; //clear volatile code	
//----------------------------------------------------------------------
 
//scan jumpers and set configuration values
	getJmp(); 
	 
//----------------------------------------------------------------------	
  //DMA settings

	//voice frame is 540 short samples with 6KHz rate (90 mS)
  //set DMA size for double buffer in circullar mode	
	
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)inbuf, 1080);  //voice recording from Mike using ADC1
  HAL_TIM_Base_Start(&htim2); //voice recording sampling rate provides by TIM3
	
  HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1,(uint32_t*)outbuf, 1080, DAC_ALIGN_12B_L); //voice playing to Speaker using DAC1
  HAL_TIM_Base_Start(&htim4); //voice playing sampling rate provides by TIM2
	
	//modem frame is 720 short samples with 8KHz rate (90 mS)
	//set DMA size for double buffer in circullar mode
	
	
	HAL_ADC_Start_DMA(&hadc2, (uint32_t*)inmdm, 1440); //baseband recording from Line using ADC2 
  HAL_TIM_Base_Start(&htim3); //baseband recording sampling rate provides by TIM8
	
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_2,(uint32_t*)outmdm, 1440, DAC_ALIGN_12B_L);
  HAL_TIM_Base_Start(&htim5); //baseband playing sampling rate provides by TIM4
	

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //====================================UART output==================================== 
		if((*tx_ptr)&&__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)) //check for we have a char and UART ready 
		{
		 (&huart2)->Instance->DR = (*tx_ptr++ & (uint8_t)0xFFU); //TX char, move pointer to next		
		}

//==================================Mike to Line processing==============================		
		if(Start_Encoding)  //check flag of speech frame was recorded
		{
		//------------------read button state-------------------------
			ptr = (uint8_t)HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13); //flag is 0 - button pressed, 1 - released
			if(ptr!=btn) //check button state was change
			{
			 btn=ptr;	//setavad neq button state
			 getJmp(); //rescan jumpers ans set configuration values		
			}
			
			//----------preprocess speech frame-----
			ptr=Start_Encoding-1; //part of double buffer will be processed (0 or 1)
			sp_ptr=inbuf+ptr*540; //pointer to recorded speech frame (540 6 KHz PCM samples)
			md_ptr=outmdm+ptr*720; //pointer to part of output buffer where modem will be outputted
			for(i=0;i<540;i++) sp_ptr[i]^=0x8000; //convert unsigned ADC result to signed short PCM 
			
       //----------------IKE mode processing-----------------
			if(!work) //check current state id IKE mode
			{
			 txkey(bits);  //generate key data, set control flag to bits[9], returns control flag (0-ctrl, 1-data)	
			 make_pkt(bits); //encrypt bits for key data or set control data, returns void	
			 if(btn^ptt) //if button press 
			 {
				 if(psk) Modulate_b(bits, md_ptr); //modulate data to line output 
				 else Modulate_p(bits, md_ptr); //modulate data to line output
				 if(key>0) key=-1; //reverse keyalarm flag after button was pressed
         if(ptr) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); //ike mode, btn press: yellow led blink
         else HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); 				 
			 }
			 else 
			 {
				 if((key>0) && ptr) Tone(md_ptr, 60); //or send tone to line output every even frame if keyalarm flag set 
			   else speech2line(sp_ptr, md_ptr); //or send native speech from Mike to line output			
			   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); //IKE mode, btn released: yellow LED=0
			 } 
			} //end of processing recorded speech frame in IKE mode
			 
					
       //---------------WORK mode processong----------------------	
			else  //WORK mode
			{
			 if(btn^ptt) //process voice to data only if button pressed
			 {
				 vad=melpe_a(bits, sp_ptr); //encode speech frame, voice flag in bits[9], returns voice flag (1-voice, 0-silency)
			   make_pkt(bits); //encrypt bits for voice data or set control data, returns void	
			   if(psk) Modulate_b(bits, md_ptr); //BPSK modulate data to line output
         else Modulate_p(bits, md_ptr); //PLS modulate data to line output
         if(vad || ptt) //for duplex mode yellow LED light only in work mode for voice					 
				 HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); //work mode, btn press: yellow LED=1				 
			 }
			 else 
			 {
				 speech2line(sp_ptr, md_ptr); //or send native speech from Mike to line output
         vad=0;	//clearerr voice flag	
         HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);	//work mode, BTN release: yellow LED=0			 
			 }
			}	 //end of processing recoreded speech frame in WORK mode		
//------------------------------------------------------------			
			
			//postprocess line output frame
			for(i=0;i<720;i++) md_ptr[i]^=0x8000; //convert signed PCM to unsigned short for DAC
			//------------synchronization----------------
			if(!ptr) //only for even frames
			{
				//adjust voice record rate by modem output level
			 i=dac2_level-720; //delta by level of Line playing buffer
			 i+=15000; //tuning ADC1 timer for sample rate of recording from Mike
			 __HAL_TIM_SET_AUTORELOAD(&htim2, i); //adjust ADC1 6KHz rate (Mike) 	
			}
			
			Start_Encoding=0;  //clear speach ready flag		
		} //end of processing recorded speech frame


//==========================Line to Speaker processing==============================
		
		if(Start_Decoding) //check flag of modem frame was recorded
		{
			//-------------preprocessing line frame-----
			ptr=Start_Decoding-1;
			md_ptr=inmdm+ptr*720; //pointer to recorded modem frame (720 8KHz PCM samples)
			sp_ptr=outbuf+ptr*540; //pointer to speaker frame will be played (540 6KHz PCM samples)
			for(i=0;i<720;i++) md_ptr[i]^=0x8000; //convert unsigned ADC result to signed short PCM

//----------------IKE mode processing-----------------			
			if(!work)
			{
			 //demodulate with soft bits
			 if(psk) i=Demodulate_b(md_ptr, bits, sbts); //demodulate line input, returns value for fine tuning recording sample rate
			 else i=Demodulate_p(md_ptr, bits, sbts); 
			 tim3period=11250-i; //fine adjust Line recording samlerate	
			 if(btn^ptt) bits[13]|=0x80; //bits[12]=btn;  //set button flag  !!!!!!!!
			 snc=bits[11]&0x40; //get flag of carrier detected
			 if(snc) check_pkt(bits); //check for control packets, set ctrl flag to MSB of bits[11], returns 1-voice, 0 control
			 i=ike_ber(bits,lbuf); //check packet for ber, output log, returns 0- we have their key otherwise 1 
       if((!i) && (!key)) key=1; //set flag for alarmed other side: we already have their key			
			 tx_ptr=lbuf; //set pointer to start of text buffer for output statictic over UART	
				//check carrier detected
			 if(snc) //carrier detected
			 {	 //process received data	
				work=rxkey(bits, sbts); //process received key data, returns current mode
				 if(key || ptr) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET); //IKE mode, carrier OK, have key: red LED=1
				 else HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET); //IKE mode, carrier OK, no key: red LED blink			 
			 }
			 else HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET); //no carrier: red LED=0
				
			 //resample line signal to speaker
			 line2speech(md_ptr, sp_ptr); //convert 720 *KHz line samples to 540 6KHz speaker samples   			 
			} //end of received modem frame processing in IKE mode
//----------------WORK mode processing-----------------
			else
			{
			 if(psk) i=Demodulate_b(md_ptr, bits, 0); //demodulate line input, returns value for fine tuning recording sample rate
			 else i=Demodulate_p(md_ptr, bits, 0); 	
			 tim3period=11250-i; //fine adjust Line recording samlerate	
			 if(btn^ptt) bits[13]|=0x80; //bits[12]=btn;  //set button flag  !!!!!!!!
			 if(vad) bits[13]|=0x40; // bits[12]+=2;	//setbuf vad flag
			 snc=bits[11]&0x40; //get flag of carrier detected	
			 if(snc) //carrier detected
       {  //increment packets only in good sync because later we can't rewind counter back but can move forward
				i=check_pkt(bits); //check for control packets, set ctrl flag to MSB of bits[11], returns 1-voice, 0 control 
				if(i) melpe_s(sp_ptr, bits); //decode received 9 bytes data to 540 samples into outbuf+540
			  else memset(sp_ptr, 0, 1080); //or set silency if control packet was received
				 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET); //carrier detected, work mode: red LED=1
			 }
       else //no carrier
			 {
				 line2speech(md_ptr, sp_ptr); //convert 720 *KHz line samples to 540 6KHz speaker samples
				 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET); //no carrier, work mode: red LED=0
			 }
       work_ber(bits, lbuf); //check packet for ber, output log 
			 tx_ptr=lbuf; //set pointer to start of text buffer for output statistic over UART			 
			} //end of received modem frame processing in work mode
			
			//postprocess line output frame
			for(i=0;i<540;i++) sp_ptr[i]^=0x8000; //convert signed PCM to unsigned short for DAC
			
			//------------synchronization----------------
			if(!ptr) //only for even frames
			{
				//autoadjust voice playing samplerate by himself 
			  i=540-dac1_level; //delta for tuning Speaker sampling rate
			  i+=15000; //tune DAC1 timer 
			  __HAL_TIM_SET_AUTORELOAD(&htim4, i); //adjust DAC1 6KHz rate (Speaker)	  
			}
			
			Start_Decoding=0; //clear line frame ready flag
		}//end of processing recorded modem frame
//================================================================================
		
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  } //end of while(1) infinite loop
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
    */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_LEFT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
    */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* ADC2 init function */
static void MX_ADC2_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
    */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc2.Init.DataAlign = ADC_DATAALIGN_LEFT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
    */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* ADC3 init function */
static void MX_ADC3_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
    */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = DISABLE;
  hadc3.Init.ContinuousConvMode = ENABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
    */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* DAC init function */
static void MX_DAC_Init(void)
{

  DAC_ChannelConfTypeDef sConfig;

    /**DAC Initialization 
    */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

    /**DAC channel OUT1 config 
    */
  sConfig.DAC_Trigger = DAC_TRIGGER_T4_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

    /**DAC channel OUT2 config 
    */
  sConfig.DAC_Trigger = DAC_TRIGGER_T5_TRGO;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM2 init function */
static void MX_TIM2_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 15000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM3 init function */
static void MX_TIM3_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 11250;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM4 init function */
static void MX_TIM4_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 15000;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* TIM5 init function */
static void MX_TIM5_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 11250;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : PC13 PC4 PC6 PC7 
                           PC8 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7 
                          |GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA6 PA7 PA9 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB10 PB14 PB5 
                           PB6 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_10|GPIO_PIN_14|GPIO_PIN_5 
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB13 PB15 PB3 
                           PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_13|GPIO_PIN_15|GPIO_PIN_3 
                          |GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_10, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_14|GPIO_PIN_6|GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_5|GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);

}

/* USER CODE BEGIN 4 */

//ADC DMA interupt: fist half of record buffer ready
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
 if(hadc == (&hadc1)) //ADC0 record voice frame 
 {
	 Start_Encoding=1; //set flag voice frame is redy
	 dac2_level=__HAL_DMA_GET_COUNTER(&hdma_dac2); //get level of dac2: Lineout (must be 720+few)
 }
 else //ADC1 record modem frame
 {
	 Start_Decoding=1; //set flag modem frame is ready
	 dac1_level=__HAL_DMA_GET_COUNTER(&hdma_dac1); //get level of dac1: speaker (must be 540+few) 
	 __HAL_TIM_SET_AUTORELOAD(&htim3, tim3period); //adjust ADC2 8KHz rate (Linein)
 }
}

//ADC DMA interupt: second half of record buffer ready
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
 if(hadc == (&hadc1)) //ADC0 record voice frame  
 {
	 Start_Encoding=2; //set flag voice frame is redy
 }
 else //ADC1 record modem frame
 {
	  Start_Decoding=2;  //set flag modem frame is ready
	  __HAL_TIM_SET_AUTORELOAD(&htim3, tim3period); //adjust ADC2 8KHz rate (Line in)
 }
}

//set configuration vaues depends Jumpers open/close
void getJmp(void)
{
	unsigned char val;
	//PC6 antiVAD: 1-on, 0-off
	if(GPIO_PIN_SET==HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6)) val=1; else val=0;
	//call modem functional to set anti-vad here!!!
	setavad(val);
	//PC8 modem type: jumper open(1) - pls, closed(0) - psk
	if(GPIO_PIN_SET==HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8)) psk=0; else psk=1;
	//PA6 ptt mode: jumper open(1) - duplex(invert btn), closed(0)- push-to-talk
	if(GPIO_PIN_SET==HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11)) ptt=0; else ptt=1;	
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
