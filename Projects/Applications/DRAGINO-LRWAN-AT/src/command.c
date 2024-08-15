/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "LoRaMac.h"
#include "Region.h"
#include "tremo_flash.h"
#include "tremo_delay.h"
#include "tremo_uart.h"
#include "tremo_system.h"
#include "command.h"
#include "lora_app.h"
#include "timer.h"
#include "LoRaMac.h"
#include "version.h"
#include "flash_eraseprogram.h"
#include "tremo_system.h"
#include "lora_app.h"
#include "tremo_gpio.h"
#include "lora_config.h"
#include "gpio_exti.h"
#include "log.h"
#include "tremo_iwdg.h"
#include "bsp.h"
#include "weight.h"

#define ARGC_LIMIT 16
#define ATCMD_SIZE (242 * 2 + 18)
#define PORT_LEN 4

#define QUERY_CMD		0x01
#define EXECUTE_CMD		0x02
#define DESC_CMD        0x03
#define SET_CMD			0x04

uint8_t password_comp(uint8_t scan_word[],uint8_t set_word[],uint8_t scan_lens,uint8_t set_len);

void weightreset(void);
static char ReceivedData[255];
static unsigned ReceivedDataSize = 0;
static uint8_t ReceivedDataPort=0;
static TimerEvent_t ATcommandsTimer;
static void OnTimerATcommandsEvent(void);
static uint8_t at_PASSWORD_comp(char *argv);
uint8_t dwelltime;
uint8_t parse_flag = 1; // XXX: default to password OK
bool atrecve_flag=0;
bool debug_flags=0;
bool message_flags=0;
bool getsensor_flags=0;

uint8_t atcmd[ATCMD_SIZE];
uint16_t atcmd_index = 0;
volatile bool g_atcmd_processing = false;

extern uint8_t product_id;
extern uint8_t fire_version;
extern uint8_t current_fre_band;
extern uint16_t power_5v_time;
extern char *band_string;
extern uint16_t payload_lens;
extern LoRaMainCallback_t *LoRaMainCallbacks;
extern uint32_t LoRaMacState;
extern TimerEvent_t MacStateCheckTimer;
extern TimerEvent_t TxDelayedTimer;
extern TimerEvent_t AckTimeoutTimer;
extern TimerEvent_t RxWindowTimer1;
extern TimerEvent_t RxWindowTimer2;
extern TimerEvent_t CheckBLETimesTimer;
extern TimerEvent_t TxTimer;
extern TimerEvent_t ReJoinTimer;
extern TimerEvent_t DownlinkDetectTimeoutTimer;
extern TimerEvent_t UnconfirmedUplinkChangeToConfirmedUplinkTimeoutTimer;

uint8_t symbtime1_value;
uint8_t symbtime2_value;
uint8_t flag1=0;
uint8_t flag2=0;
extern __IO bool ble_sleep_flags;
extern __IO bool ble_sleep_command;
extern bool sleep_status;
extern bool exit_temp;
extern bool joined_flags;
extern bool down_check;
extern bool mac_response_flag;
extern uint8_t decrypt_flag;
extern uint8_t password_len;
extern uint8_t password_get[8];
extern __IO bool uplink_data_status;
extern uint32_t APP_TX_DUTYCYCLE;
extern uint16_t REJOIN_TX_DUTYCYCLE;
extern uint8_t response_level;
extern uint8_t inmode,inmode2,inmode3;
extern uint8_t workmode;
extern uint32_t count1,count2;
extern float GapValue;
extern bool joined_finish;
		
extern bool FDR_status;
extern uint8_t RX2DR_setting_status;

extern uint8_t downlink_detect_switch;
extern uint16_t downlink_detect_timeout;

extern uint8_t confirmed_uplink_counter_retransmission_increment_switch;
extern uint8_t confirmed_uplink_retransmission_nbtrials;

extern uint8_t LinkADR_NbTrans_uplink_counter_retransmission_increment_switch;
extern uint8_t LinkADR_NbTrans_retransmission_nbtrials;
extern uint16_t unconfirmed_uplink_change_to_confirmed_uplink_timeout;

extern uint32_t uuid1_in_sflash,uuid2_in_sflash;
extern void sensors_data(void);

uint8_t write_key_in_flash_status=0,write_config_in_flash_status=0;

typedef struct {
	char *cmd;
	int (*fn)(int opt, int argc, char *argv[]);	
}at_cmd_t;

//AT functions
static int at_debug_func(int opt, int argc, char *argv[]);
static int at_reset_func(int opt, int argc, char *argv[]);
static int at_fdr_func(int opt, int argc, char *argv[]);
static int at_deui_func(int opt, int argc, char *argv[]);
static int at_appeui_func(int opt, int argc, char *argv[]);
static int at_appkey_func(int opt, int argc, char *argv[]);
static int at_devaddr_func(int opt, int argc, char *argv[]);
static int at_appskey_func(int opt, int argc, char *argv[]);
static int at_nwkskey_func(int opt, int argc, char *argv[]);
static int at_adr_func(int opt, int argc, char *argv[]);
static int at_txp_func(int opt, int argc, char *argv[]);
static int at_dr_func(int opt, int argc, char *argv[]);
static int at_dcs_func(int opt, int argc, char *argv[]);
static int at_pnm_func(int opt, int argc, char *argv[]);
static int at_rx2fq_func(int opt, int argc, char *argv[]);
static int at_rx2dr_func(int opt, int argc, char *argv[]);
static int at_rx1dl_func(int opt, int argc, char *argv[]);
static int at_rx2dl_func(int opt, int argc, char *argv[]);
static int at_jn1dl_func(int opt, int argc, char *argv[]);
static int at_jn2dl_func(int opt, int argc, char *argv[]);
static int at_njm_func(int opt, int argc, char *argv[]);
static int at_nwkid_func(int opt, int argc, char *argv[]);
static int at_fcu_func(int opt, int argc, char *argv[]);
static int at_fcd_func(int opt, int argc, char *argv[]);
static int at_class_func(int opt, int argc, char *argv[]);
static int at_join_func(int opt, int argc, char *argv[]);
static int at_njs_func(int opt, int argc, char *argv[]);
static int at_sendb_func(int opt, int argc, char *argv[]);
static int at_bsend_func(int opt, int argc, char *argv[]);
static int at_send_func(int opt, int argc, char *argv[]);
static int at_recv_func(int opt, int argc, char *argv[]);
static int at_recvb_func(int opt, int argc, char *argv[]);
static int at_buddy_func(int opt, int argc, char *argv[]);
static int at_ver_func(int opt, int argc, char *argv[]);
static int at_cfm_func(int opt, int argc, char *argv[]);
static int at_snr_func(int opt, int argc, char *argv[]);
static int at_rssi_func(int opt, int argc, char *argv[]);
static int at_tdc_func(int opt, int argc, char *argv[]);
static int at_port_func(int opt, int argc, char *argv[]);
static int at_pword_func(int opt, int argc, char *argv[]);
static int at_chs_func(int opt, int argc, char *argv[]);
#if defined( REGION_US915 ) || defined( REGION_US915_HYBRID ) || defined ( REGION_AU915 ) || defined ( REGION_CN470 )
static int at_che_func(int opt, int argc, char *argv[]);
#endif
static int at_rx1wto_func(int opt, int argc, char *argv[]);
static int at_rx2wto_func(int opt, int argc, char *argv[]);
static int at_decrypt_func(int opt, int argc, char *argv[]);
#if defined ( REGION_AU915 ) || defined ( REGION_AS923 )
static int at_dwellt_func(int opt, int argc, char *argv[]);
#endif
static int at_rjtdc_func(int opt, int argc, char *argv[]);
static int at_rpl_func(int opt, int argc, char *argv[]);
static int at_mod_func(int opt, int argc, char *argv[]);
static int at_intmod1_func(int opt, int argc, char *argv[]);
static int at_intmod2_func(int opt, int argc, char *argv[]);
static int at_intmod3_func(int opt, int argc, char *argv[]);
static int at_weigre_func(int opt, int argc, char *argv[]);
static int at_weigap_func(int opt, int argc, char *argv[]);
static int at_5vtime_func(int opt, int argc, char *argv[]);
static int at_setcount_func(int opt, int argc, char *argv[]);
static int at_sleep_func(int opt, int argc, char *argv[]);
static int at_cfg_func(int opt, int argc, char *argv[]);
static int at_uuid_func(int opt, int argc, char *argv[]);
static int at_downlink_detect_func(int opt, int argc, char *argv[]);
static int at_setmaxnbtrans_func(int opt, int argc, char *argv[]);
static int at_getsensorvalue_func(int opt, int argc, char *argv[]);
static int at_disfcntcheck_func(int opt, int argc, char *argv[]);
static int at_dismacans_func(int opt, int argc, char *argv[]);
static int at_rxdatatest_func(int opt, int argc, char *argv[]);

static at_cmd_t g_at_table[] = {
	  {AT_DEBUG, at_debug_func},
		{AT_RESET, at_reset_func},
		{AT_FDR, at_fdr_func},
	  {AT_DEUI, at_deui_func},
		{AT_APPEUI, at_appeui_func},
		{AT_APPKEY, at_appkey_func},
		{AT_DEVADDR, at_devaddr_func},
		{AT_APPSKEY, at_appskey_func},
		{AT_NWKSKEY, at_nwkskey_func},	
		{AT_ADR, at_adr_func},
		{AT_TXP, at_txp_func},
		{AT_DR, at_dr_func},
		{AT_DCS, at_dcs_func},
		{AT_PNM, at_pnm_func},
		{AT_RX2FQ, at_rx2fq_func},
		{AT_RX2DR, at_rx2dr_func},
		{AT_RX1DL, at_rx1dl_func},
		{AT_RX2DL, at_rx2dl_func},
		{AT_JN1DL, at_jn1dl_func},
		{AT_JN2DL, at_jn2dl_func},
		{AT_NJM, at_njm_func},
		{AT_NWKID, at_nwkid_func},
		{AT_FCU, at_fcu_func},
		{AT_FCD, at_fcd_func},
		{AT_CLASS, at_class_func},
		{AT_JOIN, at_join_func},
		{AT_NJS, at_njs_func},
	{AT_SENDB, at_sendb_func},
  {AT_BSEND, at_bsend_func},
	{AT_SEND, at_send_func},
		{AT_RECVB,at_recvb_func},
		{AT_RECV,at_recv_func},		
    {AT_BUDDY, at_buddy_func},
		{AT_VER, at_ver_func},
		{AT_CFM, at_cfm_func},
		{AT_SNR, at_snr_func},
		{AT_RSSI, at_rssi_func},
		{AT_TDC, at_tdc_func},
		{AT_PORT, at_port_func},
		{AT_PWORD, at_pword_func},
		{AT_CHS, at_chs_func},
		#if defined( REGION_US915 ) || defined( REGION_US915_HYBRID ) || defined ( REGION_AU915 ) || defined ( REGION_CN470 )
		{AT_CHE, at_che_func},
		#endif
		{AT_RX1WTO, at_rx1wto_func},
		{AT_RX2WTO, at_rx2wto_func},
		{AT_DECRYPT, at_decrypt_func},
		#if defined ( REGION_AU915 ) || defined ( REGION_AS923 )
		{AT_DWELLT, at_dwellt_func},
		#endif
		{AT_RJTDC, at_rjtdc_func},
		{AT_RPL, at_rpl_func},	
		{AT_MOD, at_mod_func},
		{AT_INTMOD1, at_intmod1_func},
		{AT_INTMOD2, at_intmod2_func},
		{AT_INTMOD3, at_intmod3_func},	
    {AT_WEIGRE, at_weigre_func},	
    {AT_WEIGAP, at_weigap_func},		
		{AT_5VT, at_5vtime_func},	
		{AT_SETCNT, at_setcount_func},
		{AT_SLEEP, at_sleep_func},		
		{AT_CFG, at_cfg_func},		
		{AT_UUID, at_uuid_func},		
		{AT_DDETECT, at_downlink_detect_func},
		{AT_SETMAXNBTRANS, at_setmaxnbtrans_func},
		{AT_GETSENSORVALUE, at_getsensorvalue_func},
		{AT_DISFCNTCHECK, at_disfcntcheck_func},		
		{AT_DISMACANS, at_dismacans_func},
		{AT_RXDATEST,at_rxdatatest_func},
};

#define AT_TABLE_SIZE	(sizeof(g_at_table) / sizeof(at_cmd_t))

uint8_t rxdata_change_hex(const char *hex, uint8_t *bin)
{
	uint8_t leg_temp=0;
  uint8_t flags=0;
	uint8_t *cur = bin;
  uint16_t hex_length = strlen(hex);
  const char *hex_end = hex + hex_length;
  uint8_t num_chars = 0;
  uint8_t byte = 0;
	
  while (hex < hex_end) {
        flags=0;
        if ('A' <= *hex && *hex <= 'F') {
            byte |= 10 + (*hex - 'A');
        } else if ('a' <= *hex && *hex <= 'f') {
            byte |= 10 + (*hex - 'a');
        } else if ('0' <= *hex && *hex <= '9') {
            byte |= *hex - '0';
        } else if (*hex == ' ') {
           flags=1;
        }else {
            return -1;
        }
        hex++;
        
        if(flags==0)
        {
          num_chars++;
          if (num_chars >= 2) {
              num_chars = 0;
              *cur++ = byte;
							leg_temp++;
              byte = 0;
             } else {
              byte <<= 4;
             }
        }
    }	
	
	 return leg_temp;
}

void set_at_receive(uint8_t AppPort, uint8_t* Buff, uint8_t BuffSize)
{
  if (255 <= BuffSize)
    BuffSize = 255;
  memcpy1((uint8_t *)ReceivedData, Buff, BuffSize);
  ReceivedDataSize = BuffSize;
  ReceivedDataPort = AppPort;
}

static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length)
{
	  uint8_t flags=0;
    uint16_t hex_length = strlen(hex);
    const char *hex_end = hex + hex_length;
    uint8_t *cur = bin;
    uint8_t num_chars = 0;
    uint8_t byte = 0;

    if ((hex_length + 1) / 2 > bin_length) {
       if((hex_length!=11)&&(hex_length!=23)&&(hex_length!=47))
       {
        return -1;
       }
    }

    while (hex < hex_end) {
        flags=0;
        if ('A' <= *hex && *hex <= 'F') {
            byte |= 10 + (*hex - 'A');
        } else if ('a' <= *hex && *hex <= 'f') {
            byte |= 10 + (*hex - 'a');
        } else if ('0' <= *hex && *hex <= '9') {
            byte |= *hex - '0';
        } else if (*hex == ' ') {
           flags=1;
        }else {
            return -1;
        }
        hex++;
        
        if(flags==0)
        {
          num_chars++;
          if (num_chars >= 2) {
              num_chars = 0;
              *cur++ = byte;
              byte = 0;
             } else {
              byte <<= 4;
             }
        }
    }
		
    return cur - bin;
}

static int at_debug_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS;  				
					if(debug_flags==1)
					{
						debug_flags=0;
					  snprintf((char *)atcmd, ATCMD_SIZE, "Exit Debug mode\r\n");								
					}
					else
					{
						debug_flags=1;		
					  snprintf((char *)atcmd, ATCMD_SIZE, "Enter Debug mode\r\n");						
					}
					
          break;
        }
		case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Set more info output\r\n");
			    break;
				}		
		
     default: break;
    }
    
    return ret;
}

static int at_reset_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS;
          *((uint8_t *)(0x2000F00A))=0x11;	
          delay_ms(100);					
          system_reset();
          break;
        }
				
		case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Trig a reset of the MCU\r\n");
			    break;
				}
		
     default: break;
    }
    
    return ret;
}

static int at_fdr_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS;   
					uint8_t status[128]={0};
					memset(status, 0x00, 128);
					__disable_irq();
					status[1]=product_id;
					status[2]=current_fre_band;
					status[3]=fire_version;
					
					flash_erase_page(FLASH_USER_START_ADDR_CONFIG);
					delay_ms(5);					
					if(flash_program_bytes(FLASH_USER_START_ADDR_CONFIG,status,128)==ERRNO_FLASH_SEC_ERROR)
					{
						snprintf((char *)atcmd, ATCMD_SIZE, "FDR error\r\n");
					}
					__enable_irq();
					
					delay_ms(100);
          system_reset();
          break;
        }
				
		case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Reset Parameters to Factory Default, Keys Reserve\r\n");
					break;
				}
		
     default: break;
    }
    
    return ret;
}

static int at_deui_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint8_t buf[8];
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;					
            lora_config_deveui_get(buf);                                                                                                          
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X\r\n", \
                             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);           
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 8);
            if (length == 8) {
                    lora_config_deveui_set(buf);						
                    ret = LWAN_SUCCESS; 
                    write_key_in_flash_status=1;
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Device EUI\r\n");
					break;
				}
				
        default: break;
    }
    
    return ret;
}

static int at_appeui_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint8_t buf[8];
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;					
            lora_config_appeui_get(buf);	                                                                                                           
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X\r\n", \
                             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);           
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 8);
            if (length == 8) {
                    lora_config_appeui_set(buf);
                    ret = LWAN_SUCCESS;
							      write_key_in_flash_status=1;
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Application EUI\r\n");
					break;
				}
				
        default: break;
    }
    
    return ret;
}

static int at_appkey_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint8_t buf[16];
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;					
            lora_config_appkey_get(buf);	                                                                                                            
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n", \
                             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);								
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 16);
            if (length == 16) {
                    lora_config_appkey_set(buf);                  
                    ret = LWAN_SUCCESS;
                    write_key_in_flash_status=1;							
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Application Key\r\n");
					break;
				}
				
        default: break;
    }
    
    return ret;
}

static int at_devaddr_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint32_t devaddr;
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;					
            devaddr=lora_config_devaddr_get();	                                                                                                             
            snprintf((char *)atcmd, ATCMD_SIZE, "%08X\r\n", (unsigned int)devaddr);           
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
					
            uint8_t buf[4];
            length = hex2bin((const char *)argv[0], buf, 4);
            if (length == 4) {
							      devaddr = buf[0] << 24 | buf[1] << 16 | buf[2] <<8 | buf[3];
                    lora_config_devaddr_set(devaddr);
                    ret = LWAN_SUCCESS;
							      write_key_in_flash_status=1;
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Device Address\r\n");
					break;
				}
        default: break;
    }
    
    return ret;
}

static int at_appskey_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint8_t buf[16];
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            lora_config_appskey_get(buf);                                                                                                  
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n", \
                             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);           
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 16);
            if (length == 16) {
                    lora_config_appskey_set(buf);
                    ret = LWAN_SUCCESS;   
							      write_key_in_flash_status=1;
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Application Session Key\r\n");
					break;
				}
				
        default: break;
    }
    
    return ret;
}

static int at_nwkskey_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t length;
    uint8_t buf[16];
        
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            lora_config_nwkskey_get(buf);	                                                                                                           
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n", \
                             buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);            
            break;
        }

        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 16);
            if (length == 16) {
                    lora_config_nwkskey_set(buf);
                    ret = LWAN_SUCCESS; 
                    write_key_in_flash_status=1;							
                    atcmd[0] = '\0';							
            }
						else
							ret = LWAN_PARAM_ERROR;
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Network Session Key\r\n");
					break;
				}
				
        default: break;
    }
    
    return ret;
}

static int at_adr_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t adr;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_ADR;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.AdrEnable);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            adr = strtol((const char *)argv[0], NULL, 0);
            if(adr<2)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_ADR;
							  mib.Param.AdrEnable = adr;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
									
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Adaptive Data Rate setting. (0: off, 1: on)\r\n");
					break;
				}
				
        default: break;
    }

    return ret;
}

static int at_txp_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t power;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						
            
						mib.Type = MIB_CHANNELS_TX_POWER;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.ChannelsTxPower);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            power = strtol((const char *)argv[0], NULL, 0);
            if((power<16)||((power>=40)&&(power<=52)))
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_CHANNELS_TX_POWER;
							  mib.Param.ChannelsTxPower = power;
								status=LoRaMacMibSetRequestConfirm(&mib);
								
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;					
        }
						
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Transmit Power (0-5, MAX:0, MIN:5, according to LoRaWAN Spec)\r\n");
					break;
				}	
        default: break;
    }

    return ret;
}

static int at_dr_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t datarate;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_CHANNELS_DATARATE;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.ChannelsDatarate);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            datarate = strtol((const char *)argv[0], NULL, 0);

						#if defined( REGION_AS923 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_AU915 )
						if((datarate==7)||(datarate>=14))
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_CN470 )
						if(datarate>=6)	
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_CN779 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_EU433 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_IN865 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_EU868 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;	
						}
				    #elif defined( REGION_KR920 )
						if(datarate>=6)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}
				    #elif defined( REGION_US915 )
						if(((datarate>=5)&&(datarate<=7))||(datarate>=14))
						{
						ret = LWAN_PARAM_ERROR;break;								
						}
				    #elif defined( REGION_RU864 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;			
						}	
				    #elif defined( REGION_KZ865 )
						if(datarate>=8)
						{
						ret = LWAN_PARAM_ERROR;break;							
						}			
				    #endif
		
						lora_config_tx_datarate_set(datarate) ;
						snprintf((char *)atcmd, ATCMD_SIZE, "Attention:Take effect after AT+ADR=0\r\n");
						ret = LWAN_SUCCESS;              
						write_config_in_flash_status=1;
						
            break;
        }
						
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Data Rate. (0-7 corresponding to DR_X)\r\n");
					break;
				}	
        default: break;
    }

    return ret;
}

static int at_dcs_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t status;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
            if (lora_config_duty_cycle_get() == LORA_ENABLE)											
					  {
							status=1;
						}
						else
						{
							status=0;							
						}	
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", status);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
					  ret = LWAN_SUCCESS;
					  write_config_in_flash_status=1;
					  atcmd[0] = '\0';
					
            if(status==0)
            {
                lora_config_duty_cycle_set(LORA_DISABLE);								                
            }
						else if(status==1)
						{
							  lora_config_duty_cycle_set(LORA_ENABLE);
						}
            else
            {   
                write_config_in_flash_status=0;							
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the ETSI Duty Cycle setting - 0=disable, 1=enable - Only for testing\r\n");
					break;
				}	
        default: break;
    }

    return ret;
}
	
static int at_pnm_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t status;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_PUBLIC_NETWORK;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.EnablePublicNetwork);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
            if(status<2)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status1;
								mib.Type = MIB_PUBLIC_NETWORK;
							  mib.Param.EnablePublicNetwork = status;
								status1=LoRaMacMibSetRequestConfirm(&mib);
								
							  if(status1==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the public network mode. (0: off, 1: on)\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_rx2fq_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t freq;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_RX2_CHANNEL;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.Rx2Channel.Frequency);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            freq = strtol((const char *)argv[0], NULL, 0);
            if(100000000<freq && freq<999000000)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_RX2_CHANNEL;
					      LoRaMacMibGetRequestConfirm(&mib);
							
								mib.Type = MIB_RX2_CHANNEL;
							
								mib.Param.Rx2Channel = ( Rx2ChannelParams_t ){ freq , mib.Param.Rx2Channel.Datarate };
					
								status=LoRaMacMibSetRequestConfirm(&mib);
								
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Rx2 window frequency\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_rx2dr_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t dr;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_RX2_CHANNEL;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.Rx2Channel.Datarate);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            dr = strtol((const char *)argv[0], NULL, 0);
            if(dr<16)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
						 	  mib.Type = MIB_RX2_CHANNEL;
					      LoRaMacMibGetRequestConfirm(&mib);
							
								mib.Type = MIB_RX2_CHANNEL;
							  mib.Param.Rx2Channel = ( Rx2ChannelParams_t ){ mib.Param.Rx2Channel.Frequency , dr };
								status=LoRaMacMibSetRequestConfirm(&mib);
								RX2DR_setting_status=1;
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Rx2 window data rate (0-7 corresponding to DR_X)\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_rx1dl_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t delay;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_RECEIVE_DELAY_1;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.ReceiveDelay1);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            delay = strtol((const char *)argv[0], NULL, 0);
            if(delay>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_RECEIVE_DELAY_1;
							  mib.Param.ReceiveDelay1 = delay;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
                atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the delay between the end of the Tx and the Rx Window 1 in ms\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_rx2dl_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t delay;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_RECEIVE_DELAY_2;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.ReceiveDelay2);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            delay = strtol((const char *)argv[0], NULL, 0);
            if(delay>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_RECEIVE_DELAY_2;
							  mib.Param.ReceiveDelay2 = delay;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the delay between the end of the Tx and the Rx Window 2 in ms\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_jn1dl_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t delay;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						
      
						mib.Type = MIB_JOIN_ACCEPT_DELAY_1;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.JoinAcceptDelay1);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            delay = strtol((const char *)argv[0], NULL, 0);
            if(delay>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_JOIN_ACCEPT_DELAY_1;
							  mib.Param.JoinAcceptDelay1 = delay;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Join Accept Delay between the end of the Tx and the Join Rx Window 1 in ms\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_jn2dl_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t delay;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_JOIN_ACCEPT_DELAY_2;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.JoinAcceptDelay2);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            delay = strtol((const char *)argv[0], NULL, 0);
            if(delay>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_JOIN_ACCEPT_DELAY_2;
							  mib.Param.JoinAcceptDelay2 = delay;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Join Accept Delay between the end of the Tx and the Join Rx Window 2 in ms\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_njm_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t status;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
            if (lora_config_otaa_get() == LORA_ENABLE)											
					  {
							status=1;
						}
						else
						{
							status=0;							
						}	
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", status);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
					  ret = LWAN_SUCCESS;
					  write_config_in_flash_status=1;
					  atcmd[0] = '\0';
					
            if(status==0)
            {
                lora_config_otaa_set(LORA_DISABLE);								                
            }
						else if(status==1)
						{
							  lora_config_otaa_set(LORA_ENABLE);
						}
            else
            {            
                write_config_in_flash_status=0;							
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Network Join Mode. (0: ABP, 1: OTAA)\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_nwkid_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
	  uint8_t length;
    uint8_t buf[4];
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_NET_ID;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%02X %02X %02X %02X\r\n", (uint8_t)(mib.Param.NetID>>24&0xff),(uint8_t)(mib.Param.NetID>>16&0xff),(uint8_t)(mib.Param.NetID>>8&0xff),(uint8_t)(mib.Param.NetID&0xff));

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            length = hex2bin((const char *)argv[0], buf, 4);
            if(length==4)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
								mib.Type = MIB_NET_ID;
							  mib.Param.NetID = buf[0] << 24 | buf[1] << 16 | buf[2] <<8 | buf[3];
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Network ID\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_fcu_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t counter;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_UPLINK_COUNTER;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.UpLinkCounter);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            counter = strtol((const char *)argv[0], NULL, 0);
            if(counter>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_UPLINK_COUNTER;
							  mib.Param.UpLinkCounter = counter;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Frame Counter Uplink\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_fcd_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t counter;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_DOWNLINK_COUNTER;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)mib.Param.DownLinkCounter);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            counter = strtol((const char *)argv[0], NULL, 0);
            if(counter>0)
            {
								MibRequestConfirm_t mib;								
							  LoRaMacStatus_t status;
							
								mib.Type = MIB_DOWNLINK_COUNTER;
							  mib.Param.DownLinkCounter = counter;
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
            }
            else
            {                
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Frame Counter Downlink\r\n");
					break;
				}
        default: break;
    }

    return ret;
}
	
static int at_class_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
	
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_DEVICE_CLASS;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%c\r\n", 'A' + mib.Param.Class);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;            
					
					  MibRequestConfirm_t mib;						
            LoRaMacStatus_t status;
					
						mib.Type = MIB_DEVICE_CLASS;
					
            switch (*argv[0])
						{
							case 'A':
							case 'B':
							case 'C':
								/* assume CLASS_A == 0, CLASS_B == 1, etc, which is the case for now */
								mib.Param.Class = (DeviceClass_t)(*argv[0] - 'A');
								status=LoRaMacMibSetRequestConfirm(&mib);
							
							  if(status==LORAMAC_STATUS_OK)
								{
									ret = LWAN_SUCCESS;
									write_config_in_flash_status=1;
								}
								else
									ret = LWAN_BUSY_ERROR;
								
							  atcmd[0] = '\0';
								break;
							default:
								return LWAN_PARAM_ERROR;
						} 

            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Device Class\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_join_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS; 
          atcmd[0] = '\0';					
          LORA_Join();
          break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Join network\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_njs_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case QUERY_CMD:
        {
            ret = LWAN_SUCCESS;
					 
					  MibRequestConfirm_t mib;						

						mib.Type = MIB_NETWORK_JOINED;
					  LoRaMacMibGetRequestConfirm(&mib);											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mib.Param.IsNetworkJoined);
            break;
        }
				
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get the join status\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_recv_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS; 
						if(atrecve_flag==0)
						{							
							LOG_PRINTF(LL_DEBUG,"%d:",ReceivedDataPort);
 				
						  if (ReceivedDataSize)
							{
								LOG_PRINTF(LL_DEBUG,"%s", ReceivedData);
								ReceivedDataSize = 0;
							}							
						}		
					  snprintf((char *)atcmd, ATCMD_SIZE, "\r\n");						
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Print last received data in raw format\r\n");
					break;
				}
								
        default: break;
    }

    return ret;		
}

static int at_recvb_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    
    switch(opt) {
        case QUERY_CMD: {
            ret = LWAN_SUCCESS; 
						if(atrecve_flag==0)
						{							
							LOG_PRINTF(LL_DEBUG,"%d:",ReceivedDataPort);
 				
							for (uint8_t i = 0; i < ReceivedDataSize; i++)
							{
								LOG_PRINTF(LL_DEBUG,"%02x ", ReceivedData[i]);
							}
							ReceivedDataSize = 0;
						}		
					  snprintf((char *)atcmd, ATCMD_SIZE, "\r\n");										
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Print last received data in binary format (with hexadecimal values)\r\n");
					break;
				}
								
        default: break;
    }

    return ret;		
}

static uint8_t sendbBuffer[256];

static int
at_sendb_func(int opt, int argc, char *argv[])
{
    lora_AppData_t msg = { NULL, 0, 0 };
	int ret = LWAN_PARAM_ERROR;
    char *n;
    unsigned int port;
    int length;


	switch (opt)
	{
	case SET_CMD:
        if (argc < 1) {
            break;
        }

        /*
         * port 0 is special - use default application port
         * port above 223 is disallowed
         */
        port = strtoul((const char *)argv[0], &n, 0);
        if (*n++ != ':' || port > 223)  {
            break;
        }

        length = hex2bin(n, sendbBuffer, sizeof(sendbBuffer));

        if (length < 0) {
            break;
        }

        if (port == 0) {
            port = lora_config_application_port_get();
        }

        msg.Port = port;
        msg.Buff = sendbBuffer;
        msg.BuffSize = length;

        if (LORA_SUCCESS != LORA_send(&msg, lora_config_reqack_get())) {
            ret = LWAN_ERROR;
        }

		ret = LWAN_SUCCESS;
		break;


	case DESC_CMD:
		ret = LWAN_SUCCESS;
		snprintf((char *)atcmd, ATCMD_SIZE,
          "Send hex data along with the application port\r\n");
		break;

	default:
		break;
	}

	return ret;
}


#define BSEND_QUEUE_SIZE 32
#define MAX_MSG_SIZE 51
// queue of messages to send
static lora_AppData_t sendQueue[BSEND_QUEUE_SIZE];
static TimerEvent_t BsendTimer;
static uint8_t payloadsBuffer[BSEND_QUEUE_SIZE][MAX_MSG_SIZE];
static int sendCursor = 0;
static int queueLength = 0;
static bool bSending = false;


static void do_bsending() {
  if (queueLength == 0) {
    bSending = false;
    return;
  }
  // try to send next message. If it fails, set a timer to recall this function
  if (LORA_SUCCESS != LORA_send(&sendQueue[sendCursor], lora_config_reqack_get())) {
    LOG_PRINTF(LL_DEBUG, "Failed to send frame, retrying in 1100ms\n");
  } else {
    LOG_PRINTF(LL_DEBUG, "Sent frame\n");
    // if successful, move to next message in 1.1sec
    sendCursor = (sendCursor + 1) % BSEND_QUEUE_SIZE;
    queueLength--;

    if (queueLength == 0) {
      bSending = false;
      return;
    }

  }
  
  TimerInit(&BsendTimer, do_bsending);
  TimerSetValue(&BsendTimer, 1100); // 1.1s to give time for firmware to process past the 1s send block
  TimerStart(&BsendTimer);
  return;
}
// buddy send; sends fragmented messages in accordance with Buddy spec
static int at_bsend_func(int opt, int argc, char *argv[]){
  // for now, we only have I-frames, so first bit is always 0
  // second bit is final and is 1 for the last frame, otherwise 0
  // next 6 bits are 0
  // next 50 bytes are the chunk of the payload


	int ret = LWAN_PARAM_ERROR;
  char *n;
  unsigned int port;

  if (argc < 1) {
      return ret;
  }

  /*
  * port 0 is special - use default application port
  * port above 223 is disallowed
  */
  port = strtoul((const char *)argv[0], &n, 0);
  if (*n++ != ':' || port > 223)  {
      return ret;
  }

  // argv is the payload body -- max size is 50 bytes
  // iterate over 50-byte chunks
  
  uint8_t payload_size = 0;


  for (int i = 2; i < strlen(argv[0]); i += 2) {
    // set header
    if (payload_size == 0) {
      // final?
      if (strlen(argv[0]) - 2 - i < (MAX_MSG_SIZE - 1)) {
        payloadsBuffer[sendCursor + queueLength][0] = 0b01000000;
      } else {
        payloadsBuffer[sendCursor + queueLength][0] = 0b00000000;
      }
      payload_size++;
    }
    // set payload byte
    char hex[3];
    hex[0] = argv[0][i];
    hex[1] = argv[0][i + 1];
    hex[2] = '\0';
    payloadsBuffer[sendCursor][payload_size] = strtol(hex, NULL, 16);

    payload_size++;

    // add to queue if full
    if (payload_size == MAX_MSG_SIZE || i == strlen(argv[0]) - 2) {

      sendQueue[sendCursor + queueLength].Port = port;
      sendQueue[sendCursor + queueLength].Buff = payloadsBuffer[sendCursor + queueLength];
      sendQueue[sendCursor + queueLength].BuffSize = payload_size;

      queueLength ++;


      if (!bSending) {
        bSending = true;
        do_bsending();
      }

      // reset payload
      payload_size = 0;
    }
  }

  return LWAN_SUCCESS;
}

static int
at_send_func(int opt, int argc, char *argv[])
{
    lora_AppData_t msg = { NULL, 0, 0 };
	int ret = LWAN_PARAM_ERROR;
    char *n;
    unsigned int port;

	switch (opt)
	{
	case SET_CMD:
        if (argc < 1) {
            break;
        }

        /*
         * port 0 is special - use default application port
         * port above 223 is disallowed
         */
        port = strtoul((const char *)argv[0], &n, 0);
        if (*n++ != ':' || port > 223)  {
            break;
        }

		if (port == 0) {
            port = lora_config_application_port_get();
        }

        msg.Port = port;
        msg.Buff = (uint8_t *) n;
        msg.BuffSize = strlen(n);

        if (LORA_SUCCESS != LORA_send(&msg, lora_config_reqack_get())) {
            ret = LWAN_ERROR;
        }

		ret = LWAN_SUCCESS;
		break;

	case DESC_CMD:
		ret = LWAN_SUCCESS;
		snprintf((char *)atcmd, ATCMD_SIZE,
          "Send text along with the application port\r\n");
		break;

	default:
		break;
	}

	return ret;
}

static int at_buddy_func(int opt, int argc, char *argv[]) {
  int ret = LWAN_PARAM_ERROR;

  switch (opt) {
    case QUERY_CMD:
      ret = LWAN_SUCCESS;
      snprintf((char *)atcmd, ATCMD_SIZE, "%s\r\n", "Hey buddy!");
      break;
    default:
      break;
  }
  return ret;
}

static int at_ver_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case QUERY_CMD:
        {
            ret = LWAN_SUCCESS;					 											
            snprintf((char *)atcmd, ATCMD_SIZE, "%s %s\r\n",band_string,AT_VERSION_STRING);
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get current image version and Frequency Band\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_cfm_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t mode=0,trials=7,increment_switch=0;
	 
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d,%d,%d\r\n", lora_config_reqack_get(),confirmed_uplink_retransmission_nbtrials,confirmed_uplink_counter_retransmission_increment_switch);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
					
            ret = LWAN_SUCCESS;
					  atcmd[0] = '\0';
					
					  if(argc==1)
						{
							mode = strtol((const char *)argv[0], NULL, 0);
							trials=7;
			        increment_switch=0;
							write_config_in_flash_status=1;
						}
						else if(argc==3)
						{
							mode = strtol((const char *)argv[0], NULL, 0);
							trials=strtol((const char *)argv[1], NULL, 0);
			        increment_switch=strtol((const char *)argv[2], NULL, 0);
							write_config_in_flash_status=1;
						}											  
						
						if(mode>1 || trials>7 || increment_switch>1)
						{
							write_config_in_flash_status=0;
							return LWAN_PARAM_ERROR;
						}
						
						if(mode==0)
						{
							lora_config_reqack_set(LORAWAN_UNCONFIRMED_MSG);
							confirmed_uplink_retransmission_nbtrials=trials;
							confirmed_uplink_counter_retransmission_increment_switch=0;
						}
						else if(mode==1)
						{
							lora_config_reqack_set(LORAWAN_CONFIRMED_MSG);
							confirmed_uplink_retransmission_nbtrials=trials;
							confirmed_uplink_counter_retransmission_increment_switch=increment_switch;
						}
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the confirmation mode (0-1)\r\n");
					break;
				}
        default: break;
    }
    
    return ret;
}

static int at_snr_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case QUERY_CMD:
        {
            ret = LWAN_SUCCESS;					 											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", lora_config_snr_get());
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get the SNR of the last received packet\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_rssi_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case QUERY_CMD:
        {
            ret = LWAN_SUCCESS;					 											
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", lora_config_rssi_get());
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get the RSSI of the last received packet\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_tdc_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t time=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)APP_TX_DUTYCYCLE);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            time = strtol((const char *)argv[0], NULL, 0);
            if(time>=5000)
            {
                APP_TX_DUTYCYCLE=time;
							  
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                LOG_PRINTF(LL_DEBUG,"TDC setting needs to be high than 4000ms\n\r");
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the application data transmission interval in ms\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_port_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t port=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", lora_config_application_port_get());

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            port = strtol((const char *)argv[0], NULL, 0);
            if(port<=255)
            {
                lora_config_application_port_set(port);
							  
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {              
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the application port\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_pword_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            switch(password_len)
						{	
							case 2:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X\r\n",password_get[0],password_get[1]);
                break;								
							}		
							case 3:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X\r\n",password_get[0],password_get[1],password_get[2]);
                break;								
							}	
							case 4:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X%02X\r\n",password_get[0],password_get[1],password_get[2],password_get[3]);
                break;								
							}	
							case 5:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X%02X%02X\r\n",
									       password_get[0],password_get[1],password_get[2],password_get[3],password_get[4]);
                break;								
							}	
							case 6:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X%02X%02X%02X\r\n",
									       password_get[0],password_get[1],password_get[2],password_get[3],password_get[4],password_get[5]);
                break;								
							}		
							case 7:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X%02X%02X%02X%2X\r\n",
									       password_get[0],password_get[1],password_get[2],password_get[3],password_get[4],password_get[5],password_get[6]);
                break;								
							}								
							case 8:
							{
								snprintf((char *)atcmd, ATCMD_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X\r\n", 
												password_get[0],password_get[1],password_get[2],password_get[3],password_get[4],password_get[5],password_get[6],password_get[7]);
                break;								
							}	
							default:
							break;							
						}
					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
					  uint8_t length;
					  uint8_t buf[10];
					
						length = hex2bin((const char *)argv[0], buf, 8);    
					
						if((length >= 2)&&(length <= 8))
					  {
							for(uint8_t i=0;i<8;i++)
							{
								password_get[i]=0x00;
							}
							
							password_len= length;
							
							for(uint8_t j=0;j<length;j++)
							{
								password_get[j]=buf[j];
							}
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							ret= LWAN_PARAM_ERROR;
						
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Set password,2 to 8 bytes\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_chs_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint32_t freq=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)customize_freq1_get());

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            freq = strtol((const char *)argv[0], NULL, 0);
            
					  if((100000000<freq && freq<999999999) || freq==0)
						{
							customize_freq1_set(freq);
							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							ret= LWAN_PARAM_ERROR;
           
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set Frequency (Unit: Hz) for Single Channel Mode\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

#if defined( REGION_US915 ) || defined( REGION_US915_HYBRID ) || defined ( REGION_AU915 ) || defined ( REGION_CN470 )
static int at_che_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;

		uint8_t fre;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;					 
					 
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)customize_set8channel_get());

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            fre = strtol((const char *)argv[0], NULL, 0);
            
						#if defined ( REGION_CN470 )
						if((fre>12)||((fre>=1)&&(fre<=10)))
						{
							fre=11;
							snprintf((char *)atcmd, ATCMD_SIZE, "Error Subband, must be 0 or 11,12\r\n");	
						}
						else
						{
							snprintf((char *)atcmd, ATCMD_SIZE, "Attention:Take effect after ATZ\r\n");	
						}
						#elif defined ( REGION_US915 )
						if(fre>8)
						{
							fre=1;
							snprintf((char *)atcmd, ATCMD_SIZE, "Error Subband, must be 0 ~ 8\r\n");
						}
						else
						{
							snprintf((char *)atcmd, ATCMD_SIZE, "Attention:Take effect after ATZ\r\n");	
						}	
						#elif defined ( REGION_AU915 )
						if(fre>8)
						{
							fre=1;
							snprintf((char *)atcmd, ATCMD_SIZE, "Error Subband, must be 0 ~ 8\r\n");		
						}
						else
						{
							snprintf((char *)atcmd, ATCMD_SIZE, "Attention:Take effect after ATZ\r\n");	
						}
						#else
						fre=0;
						#endif
						
						customize_set8channel_set(fre);
							
						ret= LWAN_SUCCESS;
            write_config_in_flash_status=1;
						
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set eight channels mode,Only for US915,AU915,CN470\r\n");
					break;
				}
        default: break;
    }

    return ret;
}	
#endif

static int at_rx1wto_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", symbtime1_value);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if ((value>=0)&&(value<=255))
            {
								flag1=1;
								symbtime1_value=value;
							  
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the number of symbols to detect and timeout from RXwindow1(0 to 255)\r\n");
					break;
				}
        default: break;
    }

    return ret;	
}

static int at_rx2wto_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", symbtime2_value);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if ((value>=0)&&(value<=255))
            {
								flag2=1;
								symbtime2_value=value;
							  
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the number of symbols to detect and timeout from RXwindow2(0 to 255)\r\n");
					break;
				}
        default: break;
    }

    return ret;		
}

static int at_decrypt_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", decrypt_flag);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if (value<=1)
            {
								decrypt_flag=value;
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the Decrypt the uplink payload(0:Disable,1:Enable)\r\n");
					break;
				}
        default: break;
    }

    return ret;		
}

#if defined ( REGION_AU915 ) || defined ( REGION_AS923 )
static int at_dwellt_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t status;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", dwelltime);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
            
					  if(status<2)
						{
							dwelltime=status;
							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							ret= LWAN_PARAM_ERROR;
           
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set UplinkDwellTime\r\n");
					break;
				}
        default: break;
    }

    return ret;
}
#endif

static int at_rjtdc_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint16_t interval;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", REJOIN_TX_DUTYCYCLE);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            interval = strtol((const char *)argv[0], NULL, 0);
            
					  if(interval>0 && interval<=65535)
						{
							REJOIN_TX_DUTYCYCLE=interval;
							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							ret= LWAN_PARAM_ERROR;
           
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the ReJoin data transmission interval in min\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_rpl_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t level;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", response_level);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            level = strtol((const char *)argv[0], NULL, 0);
            
					  if(level<=5)
						{
							response_level=level;
							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							ret= LWAN_PARAM_ERROR;
           
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set response level\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_mod_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", workmode);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
        
            value = strtol((const char *)argv[0], NULL, 0);
					
					  if((value>=1)&&(value<=9))
						{
							workmode=value;		
              LOG_PRINTF(LL_DEBUG,"Attention:Take effect after ATZ\r\n");							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
            }
						else
						{
							LOG_PRINTF(LL_DEBUG,"Mode of range is 1 to 9\r\n");		
							ret = LWAN_PARAM_ERROR;
						}
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the work mod\r\n");
					break;
				}
        default: break;
    }

    return ret;		
}

static int at_5vtime_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint16_t time=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%u\r\n", (unsigned int)power_5v_time);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
        
            time = strtol((const char *)argv[0], NULL, 0);
            power_5v_time=time;						
            ret = LWAN_SUCCESS;
					  write_config_in_flash_status=1;
					  atcmd[0] = '\0';

            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set extend the time of 5V power\r\n");
					break;
				}
        default: break;
    }

    return ret;	
}

static int at_intmod1_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", inmode);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if (value<=3)
            {
								inmode=value;
							  GPIO_EXTI8_IoInit(inmode);
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the trigger interrupt mode of PA8(0:Disable,1:falling or rising,2:falling,3:rising)\r\n");
					break;
				}
        default: break;
    }

    return ret;		
}

static int at_intmod2_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", inmode2);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if (value<=3)
            {
								inmode2=value;
							  GPIO_EXTI4_IoInit(inmode2);
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the trigger interrupt mode of PA4(0:Disable,1:falling or rising,2:falling,3:rising)\r\n");
					break;
				}
        default: break;
    }

    return ret;			
}

static int at_intmod3_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", inmode3);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if (value<=3)
            {
								inmode3=value;
							  GPIO_EXTI15_IoInit(inmode3);
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the trigger interrupt mode of PB15(0:Disable,1:falling or rising,2:falling,3:rising)\r\n");
					break;
				}
        default: break;
    }

    return ret;				
}

static int at_weigre_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS;					
          weightreset();
          break;
        }
				
		case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Set the weight to 0g\r\n");
			    break;
				}
		
     default: break;
    }
    
    return ret;	
}

static int at_weigap_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
	  uint32_t value1;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%.1f\r\n", GapValue);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
						uint16_t gapvalue_a;
						uint8_t  gapvalue_b;
					
            value1 = strtol((const char *)argv[0], NULL, 0);
					  gapvalue_a = value1/10;
					  gapvalue_b = value1%10;
					
						if (gapvalue_b<10)
            {
							GapValue=gapvalue_a+((float)gapvalue_b/10.0);
							Get_Weight();
              ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
            }
            else
            {
              ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or Set the GapValue of weight\r\n");
					break;
				}
        default: break;
    }

    return ret;			
}

static int at_setcount_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
	  uint32_t value_cnt=0;
    
    switch(opt) {        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						value_cnt = strtol((const char *)argv[1], NULL, 0);
					
						if (value==1)
            {
							count1=value_cnt;
              ret = LWAN_SUCCESS;
						  atcmd[0] = '\0';
            }
						else if (value==2)
            {
							count2=value_cnt;
              ret = LWAN_SUCCESS;
						  atcmd[0] = '\0';
            }						
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the count at present\r\n");
					break;
				}
        default: break;
    }

    return ret;				
}

static int at_sleep_func(int opt, int argc, char *argv[])
{
	  int ret = LWAN_PARAM_ERROR; 
    switch(opt) {		
		 case QUERY_CMD: {
				ret = LWAN_SUCCESS;					 
			 
				snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", sleep_status);

			 break;
		}
				 
    case EXECUTE_CMD:
        {
          ret = LWAN_SUCCESS; 
					
					*((uint8_t *)(0x2000F00A))=0xA8;
					
					TimerStop(&MacStateCheckTimer);
					TimerStop(&TxDelayedTimer);
					TimerStop(&AckTimeoutTimer);

					TimerStop(&RxWindowTimer1);
					TimerStop(&RxWindowTimer2);
					TimerStop(&CheckBLETimesTimer);							
					TimerStop(&TxTimer);
					TimerStop(&ReJoinTimer);
					TimerStop(&DownlinkDetectTimeoutTimer);
					TimerStop(&UnconfirmedUplinkChangeToConfirmedUplinkTimeoutTimer);
					
					ble_sleep_command=0;
					ble_sleep_flags=1;
					POWER_IoDeInit();				
					LOG_PRINTF(LL_DEBUG,"AT+PWRM2\r\n");
					delay_ms(100);	 	 
					gpio_init(GPIO_EXTI4_PORT, GPIO_EXTI4_PIN, GPIO_MODE_ANALOG);
					gpio_config_stop3_wakeup(GPIO_EXTI4_PORT, GPIO_EXTI4_PIN ,false,GPIO_LEVEL_HIGH);		
					gpio_init(GPIO_EXTI8_PORT, GPIO_EXTI8_PIN, GPIO_MODE_ANALOG);
					gpio_config_stop3_wakeup(GPIO_EXTI8_PORT, GPIO_EXTI8_PIN ,false,GPIO_LEVEL_HIGH);	
					gpio_init(GPIO_EXTI15_PORT, GPIO_EXTI15_PIN, GPIO_MODE_ANALOG);
					gpio_config_stop3_wakeup(GPIO_EXTI15_PORT, GPIO_EXTI15_PIN ,false,GPIO_LEVEL_HIGH);	
					joined_finish=0;
					sleep_status=1;
					
					snprintf((char *)atcmd, ATCMD_SIZE, "SLEEP\r\n");
					
          break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Set sleep mode\r\n");
					break;
				}
     default: break;
    }
    
    return ret;
}

static int at_cfg_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR; 
    switch(opt) {
    case EXECUTE_CMD:		
		      {
          ret = LWAN_SUCCESS; 
					atcmd[0] = '\0';
					
					if(LORA_JoinStatus () == LORA_SET)
					{
						if( ( LoRaMacState & 0x00000001 ) == 0x00000001 )
						{
							return LWAN_BUSY_ERROR;
						}
						else
						{
							LOG_PRINTF(LL_DEBUG,"\n\rStop Tx events,Please wait for all configurations to print\r\n");	
						}
					}
					else
					{
							LOG_PRINTF(LL_DEBUG,"\n\rStop Tx events,Please wait for all configurations to print\r\n");	
					}
					
					TimerStop(&MacStateCheckTimer);
					TimerStop(&TxDelayedTimer);
					TimerStop(&AckTimeoutTimer);

					TimerStop(&RxWindowTimer1);
					TimerStop(&RxWindowTimer2);
					TimerStop(&TxTimer);
					TimerStop(&ReJoinTimer);
					
					atrecve_flag=1;	
					iwdg_reload();					
					for (uint8_t num = 0; num < AT_TABLE_SIZE; num++)
					{							
						if(g_at_table[num].fn(QUERY_CMD, 0, 0)==LWAN_SUCCESS)
						{					
							LOG_PRINTF(LL_DEBUG,"AT%s=",g_at_table[num].cmd);
							if(strcmp(g_at_table[num].cmd,AT_RECVB)==0)
							{
								atrecve_flag=0;
								g_at_table[num].fn(QUERY_CMD, 0, 0);
							  atrecve_flag=1;
							}
							linkwan_serial_output(atcmd, strlen((const char *)atcmd));  
						}
						atcmd_index = 0;
						memset(atcmd, 0xff, ATCMD_SIZE);
					}    
					atrecve_flag=0;
					snprintf((char *)atcmd, ATCMD_SIZE, "\r\n");
						
					delay_ms(1000);
					
					if(LORA_JoinStatus () == LORA_SET)
					{			
						LOG_PRINTF(LL_DEBUG,"\n\rStart Tx events\r\n");
						TimerStart(&TxTimer);
					}
					else
					{						
						if(joined_flags==0)
						{		
							LOG_PRINTF(LL_DEBUG,"\n\rStart Tx events\r\n");
							TimerStart(&TxDelayedTimer);								
						}
					}	
          break;
        }							
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Print all configurations\r\n");
					break;
				}
     default: break;
    }
    
    return ret;    
}

static int at_uuid_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    
    uint8_t buf[8];
    
    switch(opt) {
                 
        case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            
            uint32_t unique_id[2];
            system_get_chip_id(unique_id);
            
            snprintf((char *)atcmd, ATCMD_SIZE, "%08X%08X\r\n", (unsigned int)unique_id[0],(unsigned int)unique_id[1]);
            
					  break;
        }
                
        case SET_CMD: {
            if(argc < 1) break;
            
            uint8_t length = hex2bin((const char *)argv[0], buf, 8);
            if (length == 8) {                                    
                    uuid1_in_sflash=buf[0]<<24|buf[1]<<16|buf[2]<<8|buf[3];
                    uuid2_in_sflash=buf[4]<<24|buf[5]<<16|buf[6]<<8|buf[7];
                    
                    atcmd[0] = '\0';
                    ret = LWAN_SUCCESS; 
                    write_key_in_flash_status=1;							
            }
            else if(length==6)
            {
                if(buf[0]==0x66&&buf[1]==0x66&&buf[2]==0x66&&buf[3]==0x66&&buf[4]==0x66&&buf[5]==0x66)
                {
                    uint32_t id[2];
                    system_get_chip_id(id);
                    uuid1_in_sflash=id[0]^id[1];
                    uuid2_in_sflash=uuid1_in_sflash&id[1];
                    atcmd[0] = '\0';
                    ret = LWAN_SUCCESS;
									  write_key_in_flash_status=1;
                }
            }
						else 
              ret = LWAN_PARAM_ERROR;
						
            break;
        }
				
				case DESC_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "Get the device Unique ID\r\n");
            break;
        }
								
        default: break;
    }

    return ret;
}

static int at_downlink_detect_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
	  uint8_t status;
	  uint16_t timeout1=0,timeout2=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d,%d,%d\r\n", downlink_detect_switch,unconfirmed_uplink_change_to_confirmed_uplink_timeout,downlink_detect_timeout);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
            timeout1 = strtol((const char *)argv[1], NULL, 0);
					  timeout2 = strtol((const char *)argv[2], NULL, 0);
					
					  if(status<2)
						{
							downlink_detect_switch=status;
							
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
							return LWAN_PARAM_ERROR;

						if(timeout1>0 && timeout1<=65535 && timeout2>0 && timeout2<=65535)
						{
							unconfirmed_uplink_change_to_confirmed_uplink_timeout=timeout1;
							downlink_detect_timeout=timeout2;
							ret = LWAN_SUCCESS;
							write_config_in_flash_status=1;
							atcmd[0] = '\0';
						}
						else
						{	
              write_config_in_flash_status=0;							
							return LWAN_PARAM_ERROR;	
						}
	
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the downlink detection\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_setmaxnbtrans_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;

	  uint8_t trials,increment_switch;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d,%d\r\n", LinkADR_NbTrans_retransmission_nbtrials,LinkADR_NbTrans_uplink_counter_retransmission_increment_switch);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            trials = strtol((const char *)argv[0], NULL, 0);
            increment_switch = strtol((const char *)argv[1], NULL, 0);				  
					
						if(trials<1 || trials>15 || increment_switch>1)
						{
							return LWAN_PARAM_ERROR;
						}

						LinkADR_NbTrans_retransmission_nbtrials=trials;
						LinkADR_NbTrans_uplink_counter_retransmission_increment_switch=increment_switch;

						ret = LWAN_SUCCESS;
						write_config_in_flash_status=1;
						atcmd[0] = '\0';

            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the max nbtrans in LinkADR\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_getsensorvalue_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t status=0;
		
    switch(opt) {
        
        case SET_CMD: {
            if(argc < 1) break;
            
            status = strtol((const char *)argv[0], NULL, 0);
            
					  if(status<2)
						{
              if(status==0)
							{		
								if(( LoRaMacState & 0x00000001 ) == 0x00000001)
								{
									return LWAN_BUSY_ERROR;
								}									
								getsensor_flags=1;	
							  sensor_t bsp_sensor_data_buff;
								BSP_sensor_Read(&bsp_sensor_data_buff,1,workmode);
								getsensor_flags=0;
								atcmd[0] = '\0';							
								ret = LWAN_SUCCESS;
							}
							else if(status==1)
							{
								if ( LORA_JoinStatus () != LORA_SET)
								{
									/*Not joined, try again later*/
									return LWAN_NO_NET_JOINED;
								}
							
								message_flags=1;
								exit_temp=0;
								uplink_data_status=1;
								atcmd[0] = '\0';
								ret = LWAN_SUCCESS;								
							}
						}
						else
							ret= LWAN_PARAM_ERROR;
           
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get current sensor value\r\n");
					break;
				}
        default: break;
    }

    return ret;
}

static int at_disfcntcheck_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", down_check);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if(value<=1)
            {
								down_check=value;
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the Downlink Frame Check(0:Enable,1:Disable)\r\n");
					break;
				}
        default: break;
    }

    return ret;			
}

static int at_dismacans_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
    uint8_t value=0;
    
    switch(opt) {
         case QUERY_CMD: {
            ret = LWAN_SUCCESS;
            snprintf((char *)atcmd, ATCMD_SIZE, "%d\r\n", mac_response_flag);

					 break;
        }
        
        case SET_CMD: {
            if(argc < 1) break;
            
            value = strtol((const char *)argv[0], NULL, 0);
						if(value<=1)
            {
								mac_response_flag=value;
                ret = LWAN_SUCCESS;
							  write_config_in_flash_status=1;
							  atcmd[0] = '\0';
            }
            else
            {
                ret = LWAN_PARAM_ERROR;
            }
            break;
        }
								
				case DESC_CMD: {
					ret = LWAN_SUCCESS;
					snprintf((char *)atcmd, ATCMD_SIZE, "Get or set the MAC ANS switch(0:Enable,1:Disable)\r\n");
					break;
				}
        default: break;
    }

    return ret;			
}
	
static int at_rxdatatest_func(int opt, int argc, char *argv[])
{
    int ret = LWAN_PARAM_ERROR;
	  uint8_t length;
    uint8_t buff[20];
        
    switch(opt) {
			
        case SET_CMD: {
            if(argc < 1) break;
            
						length=rxdata_change_hex((const char *)argv[0], buff);
					
			   		lora_AppData_t rxappData;
					  rxappData.Port = 2;
            rxappData.BuffSize = length;
						rxappData.Buff=buff;
            LoRaMainCallbacks->LORA_RxData( &rxappData );
						ret = LWAN_SUCCESS;
						snprintf((char *)atcmd, ATCMD_SIZE, "\r\n");
            break;
        }
				
        default: break;
    }
    
    return ret;	
}

// this can be in intrpt context
void linkwan_serial_input(uint8_t cmd)
{
    if(g_atcmd_processing) 
        return;
    
    if ((cmd >= '0' && cmd <= '9') || (cmd >= 'a' && cmd <= 'z') ||
        (cmd >= 'A' && cmd <= 'Z') || cmd == '?' || cmd == '+' ||
        cmd == ':' || cmd == '=' || cmd == ' ' || cmd == ',' || cmd == '-' || cmd == '~' || cmd == '!') {
        if (atcmd_index >= ATCMD_SIZE) {
            memset(atcmd, 0xff, ATCMD_SIZE);
            atcmd_index = 0;
            return;
        }
        atcmd[atcmd_index++] = cmd;
    } else if (cmd == '\r' || cmd == '\n') {
        if (atcmd_index >= ATCMD_SIZE) {
            memset(atcmd, 0xff, ATCMD_SIZE);
            atcmd_index = 0;
            return;
        }
        atcmd[atcmd_index] = '\0';
    }
}

int linkwan_serial_output(uint8_t *buffer, int len)
{
    LOG_PRINTF(LL_DEBUG,"%s", buffer);

    return 0;
}

void linkwan_at_process(void)
{
    uint8_t pwflag=0;
    char *ptr = NULL;
    int argc = 0;
    int index = 0;
    char *argv[ARGC_LIMIT];
    int ret = LWAN_ERROR;
    uint8_t *rxcmd = atcmd + 2;
    int16_t rxcmd_index = atcmd_index - 2;

    if (atcmd_index <=2 && atcmd[atcmd_index] == '\0') {
        atcmd_index = 0;
        memset(atcmd, 0xff, ATCMD_SIZE);
        return;
    }

    if (rxcmd_index <= 0 || rxcmd[rxcmd_index] != '\0') {
        return;
    }

    if (parse_flag==1) {
        g_atcmd_processing = true;

        if (strncasecmp((const char *)atcmd, "AT", 2)) { 
            goto at_end;
        }

        if (!strncasecmp((const char *)atcmd, "AT?", 4)) { // AT?
            for (uint8_t num = 0; num < AT_TABLE_SIZE; num++)
            {							
                if (g_at_table[num].fn(DESC_CMD, 0, 0) == LWAN_SUCCESS) {
                    LOG_PRINTF(LL_DEBUG,"AT%s: ",g_at_table[num].cmd);
                    linkwan_serial_output(atcmd, strlen((const char *)atcmd));  
                }
                delay_ms(50);
                atcmd_index = 0;
                memset(atcmd, 0xff, ATCMD_SIZE);
            }	
            snprintf((char *)atcmd, ATCMD_SIZE, "\r\n");
            ret=LWAN_SUCCESS;			
        } else {
            for (index = 0; index < AT_TABLE_SIZE; index++) {
                int cmd_len = strlen(g_at_table[index].cmd);
                if (!strncasecmp((const char *) rxcmd,
                  g_at_table[index].cmd, cmd_len)) {
                    ptr = (char *)rxcmd + cmd_len;
                    break;
                }
            }
            if (index >= AT_TABLE_SIZE || !g_at_table[index].fn)
                goto at_end;

            if (ptr[0] == '\0') {
                ret = g_at_table[index].fn(EXECUTE_CMD, argc, argv);
            }  else if (ptr[0] == ' ') {
                argv[argc++] = ptr;
                ret = g_at_table[index].fn(EXECUTE_CMD, argc, argv);
            } 
            else if( (ptr[0] == '?') && (ptr[1] == '\0')) {
                ret = g_at_table[index].fn(DESC_CMD, argc, argv);				
            }else if ((ptr[0] == '=') && (ptr[1] == '?') && (ptr[2] == '\0')) {
                ret = g_at_table[index].fn(QUERY_CMD, argc, argv);
            } else if (ptr[0] == '=') {
                ptr += 1;

                char *str = strtok((char *)ptr, ",");
                while(str) {
                    argv[argc++] = str;
                    str = strtok((char *)NULL, ",");
                }
                ret = g_at_table[index].fn(SET_CMD, argc, argv);				

                if(ret==LWAN_SUCCESS && write_key_in_flash_status==1)
                {
                    write_key_in_flash_status=0;				
                    Flash_store_key();
                }

                if(ret==LWAN_SUCCESS && write_config_in_flash_status==1)
                {
                    write_config_in_flash_status=0;				
                    Flash_Store_Config();
                }

            } else {
                ret = LWAN_ERROR;
            }
        }
    }
    else
    {
        if (!strncasecmp((const char *)atcmd, "AT+DEBUG", 9))
        {
            ret = g_at_table[0].fn(EXECUTE_CMD, argc, argv);         					
        }			
        else if(at_PASSWORD_comp((char *) atcmd)==1)
        {
            pwflag=1;
            ret = LWAN_SUCCESS;
        }
        else
        {
            pwflag=1;
        }

        index = 0;
    }			
at_end:
    if(pwflag==0)
    {	
        if (LWAN_ERROR == ret)
            snprintf((char *)atcmd, ATCMD_SIZE, "\r\n%s\r\n", AT_ERROR);
        else if(LWAN_PARAM_ERROR == ret)
            snprintf((char *)atcmd, ATCMD_SIZE, "\r\n%s\r\n", AT_PARAM_ERROR);
        else if(LWAN_BUSY_ERROR == ret) 
            snprintf((char *)atcmd, ATCMD_SIZE, "\r\n%s\r\n", AT_BUSY_ERROR);
        else if(LWAN_NO_NET_JOINED == ret) 
            snprintf((char *)atcmd, ATCMD_SIZE, "\r\n%s\r\n", AT_NO_NET_JOINED);


        linkwan_serial_output(atcmd, strlen((const char *)atcmd));  	

        if(ret==LWAN_SUCCESS)
        {
            LOG_PRINTF(LL_DEBUG,"\r\nOK\r\n");
        }	
    }   

    atcmd_index = 0;
    memset(atcmd, 0xff, ATCMD_SIZE);
    g_atcmd_processing = false;        
    return;
}

static uint8_t at_PASSWORD_comp(char *argv)
{
    uint8_t scan_len=0;	
    uint8_t buf[10];

    scan_len=hex2bin((const char *)argv, buf, 8);   

    if(password_comp(password_get,buf,scan_len,password_len)==1)
    {
        TimerInit( &ATcommandsTimer, OnTimerATcommandsEvent );
        TimerSetValue(  &ATcommandsTimer, 300000);//timeout=5 min
        TimerStart( &ATcommandsTimer );
        parse_flag=1;
        atcmd_index = 0;
        memset(atcmd, 0xff, ATCMD_SIZE);
        LOG_PRINTF(LL_DEBUG,"Correct Password\r\n");
        return 1;
    }
    else
    {
        parse_flag=0;
        atcmd_index = 0;
        memset(atcmd, 0xff, ATCMD_SIZE);
        LOG_PRINTF(LL_DEBUG,"Incorrect Password\n\r");
        return 0;
    }
}

static void OnTimerATcommandsEvent(void)
{
    parse_flag=0;
    TimerStop( &ATcommandsTimer );
}

void linkwan_at_init(void)
{
    atcmd_index = 0;
    memset(atcmd, 0xff, ATCMD_SIZE);
}

uint8_t password_comp(uint8_t scan_word[],uint8_t set_word[],uint8_t scan_lens,uint8_t set_len)
{
    uint8_t leng=0;
    for(uint8_t i=0;i<scan_lens;i++)
    {
        if(scan_word[i]==set_word[i])
        {
            leng++;
        }
    }

    if((leng==set_len)&&(scan_lens==set_len))
    {
        return 1;
    }
    else
    {
        return 0;		
    }
}

void weightreset(void)
{
    POWER_IoInit();
    WEIGHT_SCK_Init();
    WEIGHT_DOUT_Init();
    Get_Maopi();	
    delay_ms(500);
    Get_Maopi();
    WEIGHT_SCK_DeInit();
    WEIGHT_DOUT_DeInit();		
    POWER_IoDeInit();
}
