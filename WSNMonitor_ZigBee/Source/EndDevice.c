#include "OSAL.h"
#include "ZGlobals.h"
#include "AF.h"
#include "aps_groups.h"
#include "ZDApp.h"
#include "sapi.h"
#include "NodeCommon.h"
#include "WSNMonitorAppHw.h"
#include "nwk_util.h"
#include "OnBoard.h"
/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"
#include "ds18b20.c"
//#include "mac_radio_defs.h"
#include <string.h>
/*********************************************************************
 * MACROS
 */
/*********************************************************************
 * CONSTANTS
 */
// ADC definitions for CC2430/CC2530 from the hal_adc.c file
#if defined (HAL_MCU_CC2530)
#define HAL_ADC_REF_125V    0x00    /* Internal 1.25V Reference */
#define HAL_ADC_DEC_064     0x00    /* Decimate by 64 : 8-bit resolution */
#define HAL_ADC_DEC_128     0x10    /* Decimate by 128 : 10-bit resolution */
#define HAL_ADC_DEC_512     0x30    /* Decimate by 512 : 14-bit resolution */
#define HAL_ADC_CHN_VDD3    0x0f    /* Input channel: VDD/3 */
#define HAL_ADC_CHN_TEMP    0x0e    /* Temperature sensor */
#endif // HAL_MCU_CC2530

#define ACK_REQ_INTERVAL                    5
/******************************************************************************
 * TYPEDEFS
 */
typedef struct
{
  uint16              source;//����ýڵ��Լ�������̵�ַ
  uint16              parent;//����ýڵ�ĸ��ڵ������̵�ַ
  uint8               temp;
  uint8               voltage;
} gtwData_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */
//static uint16 parentShortAddr = NLME_GetCoordShortAddr();
//static uint16 myReportPeriod =    60000; 
//static uint8 reportFailureNr =    0;
// This list should be filled with Application specific Cluster IDs.
const  cId_t WSNMonitorApp_ClusterList[WSNMonitorApp_MAX_CLUSTERS] =
{
  WSNMonitorApp_PERIODIC_CLUSTERID,
  WSNMonitorApp_GROUP_CLUSTERID,
  WSNMonitorApp_ADDUNICAST_CLUSTERID,
  WSNMonitorApp_UNICASTTEMP_CLUSTERID,
  WSN_NODE_CONTROL_CMD_ID, 
  WSN_PERIOD_REPORT_CMD_ID
};
const SimpleDescriptionFormat_t WSNMonitorApp_SimpleDesc =
{
  WSNMonitorApp_ENDPOINT,              //  int Endpoint;
  WSNMonitorApp_PROFID,                //  uint16 AppProfId[2];
  WSNMonitorApp_DEVICEID,              //  uint16 AppDeviceId[2];
  WSNMonitorApp_DEVICE_VERSION,        //  int   AppDevVer:4;
  WSNMonitorApp_FLAGS,                 //  int   AppFlags:4;
  WSNMonitorApp_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)WSNMonitorApp_ClusterList,  //  uint8 *pAppInClusterList;
  WSNMonitorApp_MAX_CLUSTERS,          //  uint8  AppNumInClusters;
  (cId_t *)WSNMonitorApp_ClusterList   //  uint8 *pAppInClusterList;
};
endPointDesc_t WSNMonitorApp_epDesc;
/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
uint8 WSNMonitorApp_TaskID;    // Task ID for internal task/event processing
    // This variable will be received when
                          // WSNMonitorApp_Init() is called.
    devStates_t WSNMonitorApp_NwkState;
                                                                          
    uint8 WSNMonitorApp_TransID;  // This is the unique message ID (counter)

    afAddrType_t WSNMonitorApp_Periodic_DstAddr;
    afAddrType_t WSNMonitorApp_Group_DstAddr;
    afAddrType_t WSNMonitorApp_UnicastAdd_DstAddr;
    afAddrType_t WSNMonitorApp_UnicastTemp_DstAddr;
    
    aps_Group_t WSNMonitorApp_Group;
    uint8 WSNMonitorAppPeriodicCounter = 0;
    uint8 WSNMonitorAppFlashCounter = 0;
    
#define SENSOR_REPORT_CYCLE_SLOW 0x01
#define SENSOR_REPORT_CYCLE_NORMAL 0x02
#define SENSOR_REPORT_CYCLE_FREQUENCE 0x03

#define CYCLE_SLOW 60000
#define CYCLE_NORMAL 30000
#define CYCLE_FREQUENCE 10000   
    
static uint8 tempSetting  = 0x00;
static uint8 voltageSetting = 0x21;
static uint8 sleep = 0xFC;
static uint16 myReportPeriod = CYCLE_SLOW; 

/*********************************************************************
 * LOCAL FUNCTIONS
 */
void WSNApp_HandleKeys( uint8 shift, uint8 keys );
void WSNApp_MessageMSGCB( afIncomingMSGPacket_t *pckt );
void WSNMonitorApp_SendDataToGroup( void );
void WSNMonitorApp_SendAddDataUnicast( void );
void WSNMonitorApp_SendTempDataUnicast( void );

void send_EndDevice_PeriodReport(void);
int8 readTemp(void);
int8 readMyTemp(void);
uint8 readVoltage(void);

/*********************************************************************
 * NETWORK LAYER CALLBACKS
 */

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************/
void WSNMonitorApp_Init( uint8 task_id )
{ 
 // HAL_PA_LNA_RX_HGM();
  WSNMonitorApp_TaskID = task_id;
  WSNMonitorApp_NwkState = DEV_INIT;
  WSNMonitorApp_TransID = 0;
 #if defined ( BUILD_ALL_DEVICES ) //Ԥ���벻ͨ��
  if ( readCoordinatorJumper() )
    zgDeviceLogicalType = ZG_DEVICETYPE_COORDINATOR;
  else
    zgDeviceLogicalType = ZG_DEVICETYPE_ROUTER;
#endif // BUILD_ALL_DEVICES

#if defined ( HOLD_AUTO_START )//Ԥ���벻ͨ��
  ZDOInitDevice(0);
#endif
  
  //��ʼ����ʱ������MY_REPORT_EVT  Ȼ��ʼѭ��
  osal_set_event(task_id, MY_REPORT_EVT);
  //�㲥ģʽ
  WSNMonitorApp_Periodic_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
  WSNMonitorApp_Periodic_DstAddr.endPoint = WSNMonitorApp_ENDPOINT;
  WSNMonitorApp_Periodic_DstAddr.addr.shortAddr = 0xFFFF;   //�㲥ģʽ��һ��
  
  //�鲥ģʽ
  WSNMonitorApp_Group_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;
  WSNMonitorApp_Group_DstAddr.endPoint = WSNMonitorApp_ENDPOINT;
  WSNMonitorApp_Group_DstAddr.addr.shortAddr = WSNMonitorApp_TEMP_GROUP;
  WSNMonitorApp_epDesc.endPoint = WSNMonitorApp_ENDPOINT;
  WSNMonitorApp_epDesc.task_id = &WSNMonitorApp_TaskID;
  WSNMonitorApp_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&WSNMonitorApp_SimpleDesc;
  WSNMonitorApp_epDesc.latencyReq = noLatencyReqs;
 
//��Ե�ķ��͵�ַ��ʽ
WSNMonitorApp_UnicastAdd_DstAddr.addrMode = (afAddrMode_t)afAddr16Bit;
WSNMonitorApp_UnicastAdd_DstAddr.endPoint = WSNMonitorApp_ENDPOINT;
//WSNMonitorApp_UnicastAdd_DstAddr.addr.shortAddr = NLME_GetCoordShortAddr(); //���͵��ýڵ�ĸ��ڵ�
WSNMonitorApp_UnicastAdd_DstAddr.addr.shortAddr = 0x0000; //���͵�Э������
WSNMonitorApp_epDesc.endPoint = WSNMonitorApp_ENDPOINT;
WSNMonitorApp_epDesc.task_id = &WSNMonitorApp_TaskID;
WSNMonitorApp_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&WSNMonitorApp_SimpleDesc;
WSNMonitorApp_epDesc.latencyReq = noLatencyReqs;

//��Ե�ķ����¶ȷ�ʽ
WSNMonitorApp_UnicastTemp_DstAddr.addrMode = (afAddrMode_t)afAddr16Bit;
WSNMonitorApp_UnicastTemp_DstAddr.endPoint = WSNMonitorApp_ENDPOINT;
WSNMonitorApp_UnicastTemp_DstAddr.addr.shortAddr = 0x0000;
WSNMonitorApp_epDesc.endPoint = WSNMonitorApp_ENDPOINT;
WSNMonitorApp_epDesc.task_id = &WSNMonitorApp_TaskID;
WSNMonitorApp_epDesc.simpleDesc = (SimpleDescriptionFormat_t *)&WSNMonitorApp_SimpleDesc;
WSNMonitorApp_epDesc.latencyReq = noLatencyReqs;
  
  // Register the endpoint description with the AF
  afRegister( &WSNMonitorApp_epDesc );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( WSNMonitorApp_TaskID );

  // By default, all devices start out in Group 1
  WSNMonitorApp_Group.ID = 0x0001;
  osal_memcpy( WSNMonitorApp_Group.name, "Group 0x0001", 7  );
  aps_AddGroup( WSNMonitorApp_ENDPOINT, &WSNMonitorApp_Group );

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "WSNApp", HAL_LCD_LINE_1 );
#endif

}
/*********************************************************************
 * @fn      WSNMonitorApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
uint16 WSNMonitorApp_ProcessEvent( uint8 task_id, uint16 events ) //�¼�������
{
  afIncomingMSGPacket_t *MSGpkt;
  (void)task_id;  // Intentionally unreferenced parameter

  if ( events & SYS_EVENT_MSG ) //����ϵͳ��Ϣ����
  {
    MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( WSNMonitorApp_TaskID );
    while ( MSGpkt )  //������Ϣ����
    {
      switch ( MSGpkt->hdr.event )
      {
        // Received when a key is pressed
        case KEY_CHANGE:  //��ֵ�ı���Ϣ
          WSNApp_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        // Received when a messages is received (OTA) for this endpoint
        case AF_INCOMING_MSG_CMD://��Ϣ����
          WSNApp_MessageMSGCB( MSGpkt );
          break;

        // Received whenever the device changes state in the network
        case ZDO_STATE_CHANGE: //ZDO״̬�ı�
          WSNMonitorApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
          //�˴����Էֿ�if��Ȼ����ݲ�ͬ�豸����ͬ������
          if ( (WSNMonitorApp_NwkState == DEV_ZB_COORD)
              || (WSNMonitorApp_NwkState == DEV_ROUTER)
              || (WSNMonitorApp_NwkState == DEV_END_DEVICE) )
          {
            osal_start_timerEx( WSNMonitorApp_TaskID,
                              WSNMonitorApp_SEND_PERIODIC_MSG_EVT,  //���������Թ㲥�¼�
                              WSNMonitorApp_SEND_PERIODIC_MSG_TIMEOUT );//��������
          }
          else
          {
          }
          break;
        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );

      // Next - if one is available
      MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( WSNMonitorApp_TaskID );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG); //�ٴδ���ϵͳ�¼���Ϣ�������жϣ�ֱ��û��Ϊֹ��
  }
      //���ʹ�ܷ���
  if(events & MY_REPORT_EVT)
  {
    if (sleep == 0xFC) 
    {
      send_EndDevice_PeriodReport();
    }
     osal_start_timerEx(WSNMonitorApp_TaskID, MY_REPORT_EVT, myReportPeriod);
  }
  // Discard unknown events
  return 0;
}



/*********************************************************************
 * Event Generation Functions
 */
/*********************************************************************
 * @fn      WSNApp_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
void WSNApp_HandleKeys( uint8 shift, uint8 keys )
{ 

  (void)shift;  // Intentionally unreferenced parameter
  
  if ( keys & HAL_KEY_SW_1 ) // key up
  {  
  }
  else if ( keys & HAL_KEY_SW_2 )//keyright
  {
    aps_Group_t *grp;
    grp = aps_FindGroup( WSNMonitorApp_ENDPOINT, WSNMonitorApp_TEMP_GROUP );
    if ( grp )
    {
      // Remove from the group
      aps_RemoveGroup( WSNMonitorApp_ENDPOINT, WSNMonitorApp_TEMP_GROUP );
      HalLcdWriteString( "RemoveGroup", HAL_LCD_LINE_4 );
    }
    else
    {
      // Add to the flash group
      aps_AddGroup( WSNMonitorApp_ENDPOINT, &WSNMonitorApp_Group );
      HalLcdWriteString( "AddGroup", HAL_LCD_LINE_4 );
    }    
  }
  else if ( keys & HAL_KEY_SW_3 ) //keydown
  {  
  }
  else if ( keys & HAL_KEY_SW_4 ) //keyleft
  {   
  }
  else if ( keys & HAL_KEY_SW_5) //keycenter
  { 
  }
  else if ( (keys & HAL_KEY_SW_6) == 0x20) //����ӵ�S1  С���� ֻ��������  ԭ�����
  {   
     //WSNMonitorApp_SendAddDataUnicast();
     WSNMonitorApp_SendTempDataUnicast();
  }
  else if ( (keys & HAL_KEY_SW_7) == 0x40) // ����ӵ�S6  
  {   
     WSNMonitorApp_SendAddDataUnicast();
  }
  else if ( (keys & HAL_KEY_SW_8) == 0xA0)  //С���ӵ�S3  ������
  {      
    WSNMonitorApp_SendTempDataUnicast();
  }
  else if ( (keys & HAL_KEY_SW_9) == 0x60) //С���ӵ�S1  ������
  {      
    WSNMonitorApp_SendTempDataUnicast();
  }
}


/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      WSNApp_MessageMSGCB
 *
 * @brief   Data message processor callback.  This function processes
 *          any incoming data - probably from other devices.  So, based
 *          on cluster ID, perform the intended action.
 *
 * @param   none
 *
 * @return  none
 */
//��Ϣ������
void WSNApp_MessageMSGCB( afIncomingMSGPacket_t *pkt )
{ 
  char temp[16] = "temp:";
  uint16 sourceAddress;
  uint16 parentAddress;
  uint8 unicastData[2];
  switch ( pkt->clusterId )
  {  
    case WSNMonitorApp_PERIODIC_CLUSTERID:
      HalLedBlink( HAL_LED_1, 4, 50, (5000 / 4) );
      break;

    case WSNMonitorApp_GROUP_CLUSTERID:     
          //�ٵ�������ʾ��   �����鲥��Ϣ�豸�������ַ
      sourceAddress = BUILD_UINT16(pkt->cmd.Data[0], pkt->cmd.Data[1] );    
      HalLcdWriteStringValue( "groupM:", sourceAddress, 16, HAL_LCD_LINE_4);     
      break;
    case WSNMonitorApp_ADDUNICAST_CLUSTERID:
     //���յ����Խڵ�� �Ӹ��ڵ���Ϣ��ʱ�� ��3 4 ����ʾ���ڵ����� �Լ� ���ڵ������̵�ַ��Ϣ    
      sourceAddress = BUILD_UINT16(pkt->cmd.Data[0], pkt->cmd.Data[1] );
      parentAddress = BUILD_UINT16(pkt->cmd.Data[2], pkt->cmd.Data[3] );
      HalLcdWriteStringValue( "source:", sourceAddress, 16, HAL_LCD_LINE_3);
      HalLcdWriteStringValue( "parent:", parentAddress, 16, HAL_LCD_LINE_4);
        break;
     case WSNMonitorApp_UNICASTTEMP_CLUSTERID: 
       //���յ����Խڵ�� �¶���Ϣ��ʱ�� ��3,4 ����ʾ���� �¶���Ϣ �Լ� ����̵�ַ��Ϣ 
      strcat(temp,(char*)pkt->cmd.Data);
      HalLcdWriteString (temp , HAL_LCD_LINE_3);
      HalLcdWriteStringValue( "data:", pkt->srcAddr.addr.shortAddr, 16, HAL_LCD_LINE_4);
      break;
     case WSN_NODE_CONTROL_CMD_ID: 
            
     unicastData[0] = pkt->cmd.Data[0];//unicastData  ????????
     unicastData[1] = pkt->cmd.Data[1];
     if(unicastData[0] == 0xDA)
     {
       if(unicastData[1] == 0xFA)
       {
        // HalLedOnOff(HAL_LED_2, HAL_LED_MODE_OFF);
         HalLedSet (HAL_LED_2, HAL_LED_MODE_OFF);
         //�̵���
        // P1_2 = 0;           
        // HAL_TURN_OFF_RELAY();
       }
       else if (unicastData[1] == 0xFB)
       {
         //HalLedOnOff(HAL_LED_2, HAL_LED_MODE_OFF);
        HalLedSet (HAL_LED_2, HAL_LED_MODE_ON);
       //  P1_2 = 1;
         // HAL_TURN_ON_RELAY() ; 
       }
     }
     else if(unicastData[0] == 0xDB)
     {
       tempSetting = unicastData[1];
     }
     else if(unicastData[0] == 0xDC)
     {
       voltageSetting = unicastData[1];
     }
     else if(unicastData[0] == 0xDD)
     {
        sleep = 0xFD;
     }
     else if(unicastData[0] == 0xDE)
     {  
        sleep = 0xFC;
        switch ( unicastData[1] )
        {  
           case SENSOR_REPORT_CYCLE_SLOW:
             myReportPeriod = CYCLE_SLOW;
             break;
           case SENSOR_REPORT_CYCLE_NORMAL:   
             myReportPeriod = CYCLE_NORMAL;
             break;
           case SENSOR_REPORT_CYCLE_FREQUENCE:
             myReportPeriod = CYCLE_FREQUENCE;
             break;
           default :
             myReportPeriod = CYCLE_NORMAL;
             break;
       }  
     }
     break;
  }
}

/*****************************************
  *By Change_Ty �鲥����  begin
  ******************************************/ 
void WSNMonitorApp_SendDataToGroup( void )
{ 
  //�鲥�������Լ��Ķ̵�ַ������˵����˭����ȥ���鲥��Ϣ 
  uint16 sourceAddr = NLME_GetShortAddr(); 
  uint8 buffer[2];
  buffer[0] = LO_UINT16(sourceAddr);
  buffer[1] = HI_UINT16(sourceAddr);
  
  if ( AF_DataRequest( &WSNMonitorApp_Group_DstAddr, //����Ŀ�ĵ�ַ+�˵��ַ+����ģʽ
                       &WSNMonitorApp_epDesc, //��������
                       WSNMonitorApp_GROUP_CLUSTERID,//��ID
                       2, //��Ч���ݳ���                           
                       buffer, //����
                       &WSNMonitorApp_TransID,
                       AF_DISCV_ROUTE, //·�ɷ�ʽѡ��
                       AF_DEFAULT_RADIUS //·�����
                         ) == afStatus_SUCCESS )
  {
    
  }
  else
  {
    // Error occurred in request to send.
  }
}
  /*****************************************
  *By Change_Ty �鲥���� end.
  ******************************************/

/*****************************************
  *By Change_Ty ���͵�ַ��Ϣ����  begin
  ******************************************/
void WSNMonitorApp_SendAddDataUnicast( void )
{
 
  uint16 sourceAdd = NLME_GetShortAddr();
  uint16 parentAdd = NLME_GetCoordShortAddr();
  uint8 buffer[4];
  buffer[0] = LO_UINT16( sourceAdd );
  buffer[1] = HI_UINT16( sourceAdd );
  buffer[2] = LO_UINT16( parentAdd );
  buffer[3] = HI_UINT16( parentAdd );
  if ( AF_DataRequest( &WSNMonitorApp_UnicastAdd_DstAddr, //����Ŀ�ĵ�ַ+�˵��ַ+����ģʽ
                       &WSNMonitorApp_epDesc, //�˵�������
                       WSNMonitorApp_ADDUNICAST_CLUSTERID ,//��ID
                       4,
                       buffer,
                       &WSNMonitorApp_TransID,
                       AF_DISCV_ROUTE, //·�ɷ�ʽѡ��
                       AF_DEFAULT_RADIUS //·�����
                         ) == afStatus_SUCCESS )
  {
    
  }
  else
  {
    // Error occurred in request to send.
  }
}
/*****************************************
  *By Change_Ty ���͵�ַ��Ϣ���� end
  ******************************************/

/*****************************************
  *By Change_Ty �����¶���Ϣ����  begin
  ******************************************/
void WSNMonitorApp_SendTempDataUnicast( void )
{
  
  Init_DS18B20();
  ReadTemperature();
  gettempstr();
  
 
  if ( AF_DataRequest( &WSNMonitorApp_UnicastTemp_DstAddr, //����Ŀ�ĵ�ַ+�˵��ַ+����ģʽ
                       &WSNMonitorApp_epDesc, //�˵�������
                       WSNMonitorApp_UNICASTTEMP_CLUSTERID ,//��ID
                      (byte)osal_strlen( ch ) + 1, //��Ч���ݳ���                           
                      (byte *)&ch, //����
                       &WSNMonitorApp_TransID,
                       AF_DISCV_ROUTE, //·�ɷ�ʽѡ��
                       AF_DEFAULT_RADIUS //·�����
                         ) == afStatus_SUCCESS )
  {
    
  }
  else
  {
    // Error occurred in request to send.
  }
}

/*****************************************
  *By Change_Ty �����¶���Ϣ����  end
  ******************************************/


void send_EndDevice_PeriodReport(void)
{
  
  uint8 pData[SENSOR_REPORT_LENGTH];
  //static uint8 reportNr=0;
  //uint8 txOptions;
  
 
  
  pData[SENSOR_TEMP_OFFSET] =  readMyTemp();
  //ʹ������ĺ��� ������ʧ��?
  //pData[SENSOR_TEMP_OFFSET] =  readTemp(); // Read and report temperature value
  pData[SENSOR_VOLTAGE_OFFSET] = readVoltage(); // Read and report voltage value
    
  pData[SENSOR_PARENT_OFFSET] =  HI_UINT16(NLME_GetCoordShortAddr());
  
  pData[SENSOR_PARENT_OFFSET + 1] =  LO_UINT16(NLME_GetCoordShortAddr());
  
  pData[SENSOR_PARENT_OFFSET + 2] = aExtendedAddress[7];
  pData[SENSOR_PARENT_OFFSET + 3] = aExtendedAddress[6];
  pData[SENSOR_PARENT_OFFSET + 4] = aExtendedAddress[5];
  pData[SENSOR_PARENT_OFFSET + 5] = aExtendedAddress[4];
  pData[SENSOR_PARENT_OFFSET + 6] = aExtendedAddress[3];
  pData[SENSOR_PARENT_OFFSET + 7] = aExtendedAddress[2];
  pData[SENSOR_PARENT_OFFSET + 8] = aExtendedAddress[1];
  pData[SENSOR_PARENT_OFFSET + 9] = aExtendedAddress[0];
  
  //����¶�С�� tempSetting �򲻷���.
  if((pData[SENSOR_TEMP_OFFSET] >= tempSetting) || (pData[SENSOR_VOLTAGE_OFFSET] <= voltageSetting))
  {
      AF_DataRequest( &WSNMonitorApp_UnicastAdd_DstAddr, //����Ŀ�ĵ�ַ+�˵��ַ+����ģʽ
                           &WSNMonitorApp_epDesc, //�˵�������
                           WSN_PERIOD_REPORT_CMD_ID ,//���� CMD_ID ����һ��� CMD_ID
                           SENSOR_REPORT_LENGTH,
                           pData,
                           &WSNMonitorApp_TransID,
                           AF_DISCV_ROUTE, //·�ɷ�ʽѡ��
                           AF_DEFAULT_RADIUS //·�����
                             ) ;
  }
}

/******************************************************************************
 * @fn          readTemp
 *
 * @brief       read temperature from ADC
 *
 * @param       none
 *              
 * @return      temperature
 */

int8 readTemp(void)
{
  
   uint16 voltageAtTemp22;
   uint8 bCalibrate=TRUE; // Calibrate the first time the temp sensor is read
  uint16 value;
  int8 temp;

  #if defined (HAL_MCU_CC2530)
  ATEST = 0x01;
  TR0  |= 0x01; 
  
  
  ADCIF = 0;

  ADCCON3 = (HAL_ADC_REF_125V | HAL_ADC_DEC_512 | HAL_ADC_CHN_TEMP);

  while ( !ADCIF );

 
  value = ADCL;
  value |= ((uint16) ADCH) << 8;

  // Use the 12 MSB of adcValue
  value >>= 4;
  
  
  #define VOLTAGE_AT_TEMP_25        1480
  #define TEMP_COEFFICIENT          4

  // Calibrate for 22C the first time the temp sensor is read.
  // This will assume that the demo is started up in temperature of 22C
  if(bCalibrate) {
    voltageAtTemp22=value;
    bCalibrate=FALSE;
  }
  
  temp = 22 + ( (value - voltageAtTemp22) / TEMP_COEFFICIENT ); 
  // Set 0C as minimum temperature, and 100C as max
  if( temp >= 100) 
  {
    return 100;
  }
  else if (temp <= 0) {
    return 0;
  }
  else { 
    return temp;
  }
  // Only CC2530 is supported
  #else
  return 0;
  #endif
  
}

int8 readMyTemp(void)
{
  ReadTemperature();
  return temp_data[1];
}
/******************************************************************************
 * @fn          readVoltage
 * @brief       read voltage from ADC
 * @param       none             
 * @return      voltage
 */
uint8 readVoltage(void)
{
  #if defined (HAL_MCU_CC2530)
  uint16 value;

  // Clear ADC interrupt flag 
  ADCIF = 0;

  ADCCON3 = (HAL_ADC_REF_125V | HAL_ADC_DEC_128 | HAL_ADC_CHN_VDD3);

  // Wait for the conversion to finish 
  while ( !ADCIF );

  // Get the result
  value = ADCL;
  value |= ((uint16) ADCH) << 8;

  
  // value now contains measurement of Vdd/3
  // 0 indicates 0V and 32767 indicates 1.25V
  // voltage = (value*3*1.25)/32767 volts
  // we will multiply by this by 10 to allow units of 0.1 volts
  value = value >> 6;   // divide first by 2^6
  value = (uint16)(value * 37.5);
  value = value >> 9;   // ...and later by 2^9...to prevent overflow during multiplication

  return value;
  #else
  return 0;
  #endif // CC2530
}