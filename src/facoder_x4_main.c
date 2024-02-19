/*
  Name       : FA-CODER Sample program of "General Purpose Absolute-Encoder Master Interface for 8bit data"
  Device     : RZ/T2M
  Limitation : Number of communications to encoder (max 2^24 times)
               (If you want to remove this restriction, please contact Shimafuji Electric Inc.)

  Copyright (c) 2024 Shimafuji Electric Inc. All rights reserved.
*/

#include <stdio.h>
#include <stdlib.h>
#include "bsp_api.h"
#include "hal_data.h"
#include "r_ecl_rzt2_if.h"

#define SF_FAC_VERSION "20240215"

#define SFM_CHECK_ERROR(x) {if(R_ECL_SUCCESS!=(x)){printf("error: %lX\n",(x));}}

#define SF_FREQ_12_5MHz   (12500UL) // 12.500MHz
#define SF_FREQ_20_0MHz   (20000UL) // 20.000MHz
#define SF_FREQ_25_0MHz   (25000UL) // 25.000MHz
#define SF_FREQ_40_0MHz   (40000UL) // 40.000MHz
#define SF_FREQ_50_0MHz   (50000UL) // 50.000MHz

#define SF_ENC_BR_2_5MHz (0U)
#define SF_ENC_BR_4MHz   (1U)
#define SF_ENC_BR_5MHz   (2U)
#define SF_ENC_BR_8MHz   (3U)
#define SF_ENC_BR_10MHz  (4U)


struct st_gpenc8b{
                            /* A011C100 RW */  uint8_t   CTRL;
                            /* A011C101 RW */  uint8_t   BR;
                            /* A011C102 RW */  uint8_t   RXCRC;
  uint8_t dummy03[      1]; /* A011C104 RW */  uint8_t   T0;
                            /* A011C105 RW */  uint8_t   T1;
                            /* A011C106 RW */  uint8_t   T2;
  uint8_t dummy06[      1]; /* A011C108 RW */  uint8_t   TOT0;
                            /* A011C109 RW */  uint8_t   TOT1;
  uint8_t dummy08[      2]; /* A011C10C RW */  uint8_t   TXTRG;
                            /* A011C10D RW */  uint8_t   CLR;
  uint8_t dummy0A[      2]; /* A011C110 RW */  uint32_t  TXD;
  uint8_t dummy13[   1004]; /* A011C500 R  */  uint16_t  STAT;
  uint8_t dummy14[      2]; /* A011C504 R  */  uint8_t   RXRSLT0;
                            /* A011C505 R  */  uint8_t   RXRSLT1;
                            /* A011C506 R  */  uint8_t   RXRSLT2;
                            /* A011C507 R  */  uint8_t   RXRSLT3;
  uint8_t dummy19[    504]; /* A011C700 R  */  uint32_t  TICKET;
};

#define gpenc8b_0     (*(volatile struct st_gpenc8b *)(0xA011C100))
#define gpenc8b_1     (*(volatile struct st_gpenc8b *)(0xA031C100))

#define ENCIF0_VER_MAJ (*(volatile uint32_t *)(0xA01D2300))
#define ENCIF0_VER_MIN (*(volatile uint32_t *)(0xA01D2304))
#define ENCIF1_VER_MAJ (*(volatile uint32_t *)(0xA03D2300))
#define ENCIF1_VER_MIN (*(volatile uint32_t *)(0xA03D2304))

#define ENCIF0_FIFODATA (*(volatile uint16_t *)(0xA0FD3000))
#define ENCIF1_FIFODATA (*(volatile uint16_t *)(0xA0FD3200))

struct st_encif_rxd{
  uint32_t result;
  uint32_t rxd[16];
};

uint8_t g_encEn;
uint8_t g_rxNum;

struct st_encif_rxd encif_rxd[4];

uint8_t g_dmac_rxdata[16*4]={
0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA,
0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA,
0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA,
0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA, 0xAA,0xAA,0xAA,0xAA,
};

struct st_cmd{
  uint32_t txData;
  uint8_t  txNum;
  uint8_t  rxNum;
};

struct st_cmd cmd[]={
  /* id           txData, txNum, rxNum */
  /*  0 */  { 0x00000002,     1,     6, },
  /*  1 */  { 0x0000008A,     1,     6, },
  /*  2 */  { 0x00000092,     1,     4, },
  /*  3 */  { 0x0000001A,     1,    11, },
  /*  D */  { 0x00EA00EA,     3,     4, },
};

extern uint8_t g_enc_config_dat[];
extern uint32_t g_pinmux_config[];


#ifdef __ICCARM__
#pragma type_attribute=__irq __arm 
void enc_ch0_int_isr(void);
#pragma type_attribute=__irq __arm 
void enc_ch1_int_isr(void);
#endif /* __ICCARM__ */

#ifdef __CC_ARM
__irq void enc_ch0_int_isr(void);
__irq void enc_ch1_int_isr(void);
#else
#ifdef __GNUC__
void enc_ch0_int_isr(void);
void enc_ch1_int_isr(void);
#endif /* __GNUC__ */
#endif /* __CC_ARM */

static void show_versions(void);
static void show_result(void);
static void timer_start(uint32_t us);
int32_t enc_main(void);

extern transfer_info_t g_transfer0_info;
volatile bool g_dmac_transfer_complete = false;

/*
 * ********************************************************************************
 */

void user_dmac_callback (dmac_callback_args_t * p_args)
{
  uint32_t i;

  if(p_args->event == DMAC_EVENT_TRANSFER_END){

    for(i=0;i<g_rxNum;i++){
      if(g_encEn&0x0C){
        // FIFO data is 4 Byte/frame
        encif_rxd[0].rxd[i] = g_dmac_rxdata[i*4];
        encif_rxd[1].rxd[i] = g_dmac_rxdata[i*4+1];
        encif_rxd[2].rxd[i] = g_dmac_rxdata[i*4+2];
        encif_rxd[3].rxd[i] = g_dmac_rxdata[i*4+3];
      }else{
        // FIFO data is 2 Byte/frame
        encif_rxd[0].rxd[i] = g_dmac_rxdata[i*2];
        encif_rxd[1].rxd[i] = g_dmac_rxdata[i*2+1];
      }
    }

    g_dmac_transfer_complete = true;
  }
}

void enc_ch0_int_isr(void)
{
  encif_rxd[0].result = gpenc8b_0.RXRSLT0;
  encif_rxd[1].result = gpenc8b_0.RXRSLT1;
  encif_rxd[2].result = gpenc8b_0.RXRSLT2;
  encif_rxd[3].result = gpenc8b_0.RXRSLT3;

  gpenc8b_0.CLR = (uint8_t)gpenc8b_0.STAT; // INT clear
  __DMB();
}

void enc_ch1_int_isr(void)
{
  __DMB();
}


static void show_versions(void)
{
  uint32_t major=0,minor=0;

  major=ENCIF0_VER_MAJ;
  minor=ENCIF0_VER_MIN;

  printf("[Versions]\n");
  printf("  FA-CODER sample : %s\n",SF_FAC_VERSION);
  printf("  EC-lib          : %08lX\n",R_ECL_GetVersion());
  printf("  GPENCIF-8b      : %08lX_%08lX\n",major,minor);
}


static void timer_start(uint32_t us)
{
    uint32_t count;
    
    if((0xFFFFFFFF/400u)<us){
      count = 0xFFFFFFFF;
    }else{
      count = us*400u; // set time [us]
    }
    
    R_GPT_PeriodSet(&g_timer0_ctrl, count);
    R_GPT_Start(&g_timer0_ctrl);

    return;
}

static void show_result(void)
{
  uint32_t i,j;

  for(j=0;j<4;j++){
    printf("  [%ld] reslt=%02lX, rx data=",j,encif_rxd[j].result);
    for(i=0;i<g_rxNum;i++){
      printf("%02lX,",encif_rxd[j].rxd[i]);
    }
    printf("\n");
  }
  printf("       stat=%02X,  ticket=%06lX\n",gpenc8b_0.STAT, gpenc8b_0.TICKET);

  return;
}


int32_t enc_main(void)
{
  transfer_info_t transfer_info_0;
  uint8_t  id   = R_ECL_CH_0;
  uint32_t freq = SF_FREQ_50_0MHz;
  uint8_t  rxNum_dma;
  uint32_t i,cnt;

  printf("sample program start\n");

  R_DMAC_Open(&g_transfer0_ctrl, &g_transfer0_cfg);

  SFM_CHECK_ERROR(R_ECL_Initialize());
  SFM_CHECK_ERROR(R_ECL_ConfigurePin(g_pinmux_config));
  SFM_CHECK_ERROR(R_ECL_Configure(id, g_enc_config_dat));
  SFM_CHECK_ERROR(R_ECL_Start(id, freq));

  show_versions();

  // Enable IRQ interrupt
  __enable_irq();
  __ISB();

  R_BSP_IrqDisable(VECTOR_NUMBER_ENCIF_INT0);
  R_BSP_IrqCfg(VECTOR_NUMBER_ENCIF_INT0, 11, NULL);
  R_BSP_IrqEnable(VECTOR_NUMBER_ENCIF_INT0);    

  // init param
  gpenc8b_0.T0    = 0x02; // T0  = 2*80= 160[ns]
  gpenc8b_0.T1    = 0x02; // T1  = 2*80= 160[ns]
  gpenc8b_0.T2    = 0x03; // T2  = 3*80= 240[ns]
  gpenc8b_0.TOT0  = 0x41; // TOT0=65*80=5200[ns]
  gpenc8b_0.TOT1  = 0x19; // TOT1=25*80=2000[ns]
  gpenc8b_0.RXCRC = 0x01; // CRC = x^8+1
  gpenc8b_0.BR    = SF_ENC_BR_2_5MHz; // Bitrate = 2.5Mbps
//  gpenc8b_0.BR    = SF_ENC_BR_5MHz; // Bitrate = 5Mbps

  g_encEn=0x07; // ENC enable=4b'0111 (encoder channel 2,1,0)
  gpenc8b_0.CTRL  = g_encEn|0xA0; // 4b'1010 : CRC check=on(=1), INT mode=level(=0), INT=on(=1), ELCIN=off(=0)

  transfer_info_0        = g_transfer0_info;


  // manual trigger
  for(i=0;i<sizeof(cmd)/sizeof(struct st_cmd);i++){
    printf("cmd=%ld,txNum=%d,rxNum=%2d,txData=%08lX\n",i,cmd[i].txNum,cmd[i].rxNum,cmd[i].txData);

    g_dmac_transfer_complete=false;
    
    transfer_info_0.p_src  = (void *)(&ENCIF0_FIFODATA);
    transfer_info_0.p_dest = (void *)(&g_dmac_rxdata[0]);
    g_rxNum=cmd[i].rxNum; // for dma isr
    if(g_encEn&0x0C){ rxNum_dma = g_rxNum*4; // FIFO data is 4 Byte/frame
    }else           { rxNum_dma = g_rxNum*2; // FIFO data is 2 Byte/frame
    }
    transfer_info_0.length = rxNum_dma;
    transfer_info_0.mode = TRANSFER_MODE_NORMAL;
    R_DMAC_Reconfigure(&g_transfer0_ctrl, &transfer_info_0);

    gpenc8b_0.TXD   = cmd[i].txData;
    gpenc8b_0.TXTRG = (cmd[i].rxNum<<4)|cmd[i].txNum; // start

    while (!g_dmac_transfer_complete){}
    show_result();
  }


  // ELC trigger
  i=0; // cmd=0
  g_dmac_transfer_complete=false;
  transfer_info_0.p_src  = (void *)(&ENCIF0_FIFODATA);
  transfer_info_0.p_dest = (void *)(&g_dmac_rxdata[0]);
  g_rxNum=cmd[i].rxNum; // for dma isr
  if(g_encEn&0x0C){ rxNum_dma = g_rxNum*4; // FIFO data is 4 Byte/frame
  }else           { rxNum_dma = g_rxNum*2; // FIFO data is 2 Byte/frame
  }
  transfer_info_0.length = rxNum_dma;
  transfer_info_0.mode = TRANSFER_MODE_NORMAL;
  R_DMAC_Reconfigure(&g_transfer0_ctrl, &transfer_info_0);

  // set ENCIF
  gpenc8b_0.CTRL  = g_encEn|0xB0; // 4b'1011 : CRC check=on(=1), INT mode=level(=0), INT=on(=1), ELCIN=on(=1)
  gpenc8b_0.TXD   = cmd[i].txData;
  gpenc8b_0.TXTRG = (cmd[i].rxNum<<4)|cmd[i].txNum;

  // ELC timer start
  timer_start(100*1000/* [us] */);

  cnt=0;
  while(1){

    while (!g_dmac_transfer_complete){}
    g_dmac_transfer_complete=false;
    R_DMAC_Reconfigure(&g_transfer0_ctrl, &transfer_info_0);

    printf("[%5ld] (result,data)=(%02lX,%06lX),(%02lX,%06lX),(%02lX,%06lX),(%02lX,%06lX),stat=%04X,ticket=%06lX\n",cnt,
           encif_rxd[0].result, (encif_rxd[0].rxd[4]<<16)|(encif_rxd[0].rxd[3]<<8)|encif_rxd[0].rxd[2],
           encif_rxd[1].result, (encif_rxd[1].rxd[4]<<16)|(encif_rxd[1].rxd[3]<<8)|encif_rxd[1].rxd[2],
           encif_rxd[2].result, (encif_rxd[2].rxd[4]<<16)|(encif_rxd[2].rxd[3]<<8)|encif_rxd[2].rxd[2],
           encif_rxd[3].result, (encif_rxd[3].rxd[4]<<16)|(encif_rxd[3].rxd[3]<<8)|encif_rxd[3].rxd[2],
           gpenc8b_0.STAT, gpenc8b_0.TICKET
           );
    cnt++;
  }


  return(0);
}
