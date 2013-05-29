#include <ioCC2530.h>

#ifndef DS18B20_H_
#define DS18B20_H_


#define SEARCH_ROM      0xF0              //����ROM
#define READ_ROM        0x33              //��ROM
#define MATCH_ROM       0x55              //ƥ��ROM(�Ҷ��DS18B20ʱʹ��)
#define SKIP_ROM        0xCC              //����ƥ��ROM(����DS18B20ʱ����)
#define ALARM_SEARCH    0xEC              //��������

#define CONVERT_T       0x44              //��ʼת���¶�
#define WR_SCRATCHPAD   0x4E              //д���
#define RD_SCRATCHPAD   0xBE              //�����
#define CPY_CCTATCHPAD  0x48              //���Ʊ��
#define RECALL_EE       0xB8              //δ����
#define RD_PWR_SUPPLY   0xB4              //����Դ��Ӧ



#define BT(n)    1<<(n)
#define DQIO_PORT  1
#define DQIO_PIN   0 
#define Out_DQ  P1DIR |= BT(DQIO_PIN)
#define In_DQ   P1DIR &=~BT(DQIO_PIN)
#define Low_DQ  P1_0 = 0
#define Hign_DQ P1_0 = 1
#define Read_DQ P1_0



extern void Delay_temp(uint16 timeout);
extern void Init_DS18B20(void);
extern char ReadOneChar(void);
extern void WriteOneChar(char dat);
extern void ReadTemperature(void);
extern void gettempstr(void);

#endif