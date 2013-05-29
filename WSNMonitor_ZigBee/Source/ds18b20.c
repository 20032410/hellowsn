#include "ds18b20.h"

unsigned char FRACTION_INDEX[16] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9 };//С��ֵ��ѯ��
unsigned char temp_data[2];
extern char ch[10]="asdf";



void Delay_temp(uint16 timeout)
{
    timeout>>= 1;
    while(timeout--)
    {
        
        
    }
}




void Init_DS18B20(void)
{ 
  Out_DQ;				//�������
  Low_DQ;				//����������
  Delay_temp(800);;			//480-960us
  Hign_DQ;			//����������
  Delay_temp(40);			//15-60us
  In_DQ;
  while(Read_DQ);		//��ȡ�����ߵ�ƽ������ʼ���Ƿ�����
  Out_DQ; 
  Hign_DQ;
  Delay_temp(800);
  Hign_DQ;
}



//****************************************************************************************
//*������char ReadOneChar(void)
//*���ܣ�DS18B20��ʱ��Ƭ����
//***************************************************************************************/
char ReadOneChar(void)
{    
    char i;
	char dat = 0;
	
	for(i=8;i>0;i--)
	{   
	    Out_DQ;       //�������
        Hign_DQ;
		Delay_temp(1);     //5us
	    Low_DQ;       //�õ͵�ƽ
		Delay_temp(1);
		dat >>= 1;    //����һλ
		Hign_DQ;      //�øߵ�ƽ
		asm("nop");
		In_DQ;        //��������
		Delay_temp(1);
		if(Read_DQ)   //��ȡ��ƽ
                {
		dat |= 0x80;
                }
        Delay_temp(100);
	}

	return(dat);
}

//****************************************************************************************
//*������void WriteOneChar(char dat)
//*���ܣ�DS18B20дʱ��Ƭ����
//***************************************************************************************/
void WriteOneChar(char dat)
{   
    char i;
	Out_DQ;			//�������
	for(i=8;i>0;i--)
	{
	    Hign_DQ;
	    Low_DQ;		//�õ͵�ƽ
		Delay_temp(1);
		if(dat&0x01)//д��ƽ
              {
        Hign_DQ;	//�øߵ�ƽ
		Delay_temp(80);
              }
        else
              {
         Low_DQ;	//�õ͵�ƽ
		 Delay_temp(80);
              }
		Hign_DQ;
		dat >>= 1;  //����һλ
		Delay_temp(1);
	}
	Hign_DQ;
}

//****************************************************************************************
//*������void ReadTemperature(void)
//*���ܣ���ȡ�¶�
//***************************************************************************************/
void ReadTemperature(void)
{
        unsigned char tem_h,tem_l;
        unsigned char a,b;            //��ʱ����
        unsigned char flag;           //�¶�������ǣ���Ϊ0����Ϊ1
	Init_DS18B20();					//��ʼ��
	WriteOneChar(0xCC); 			// ���������кŵĲ���
	WriteOneChar(0x44); 			// �����¶�ת��
        Delay_temp(200);
	Delay_temp(200);
	Delay_temp(200);
	
	Init_DS18B20();					//�ٴγ�ʼ��
	WriteOneChar(0xCC); 			//���������кŵĲ���
	WriteOneChar(0xBE); 			//��ȡ�¶ȼĴ�����ǰ���������¶�

	tem_l= ReadOneChar();   	//��ȡ�¶�ֵ��λLSB
	tem_h= ReadOneChar();    //��ȡ�¶�ֵ��λMSB
        
        if(tem_h & 0x80)
    {
        flag = 1;                 //�¶�Ϊ��
        a = (tem_l>>4);           //ȡ�¶ȵ�4λԭ��
        b = (tem_h<<4)& 0xf0;     //ȡ�¶ȸ�4λԭ��
        tem_h = ~(a|b) + 1;       //ȡ����������ֵ��������λ
        
        tem_l = ~(a&0x0f) + 1;    //ȡС������ԭֵ����������λ
    }
    else
    {
        flag = 0;                 //Ϊ��
        a = tem_h<<4;
        a += (tem_l&0xf0)>>4;     //�õ���������ֵ 
        b = tem_l&0x0f;           //�ó�С������ֵ
        tem_h = a;                //��������
        tem_l = b&0xff;           //С������
    }
    
    temp_data[0] = FRACTION_INDEX[tem_l]; //����С��ֵ
    temp_data[1] = tem_h| (flag<<7);      //�������֣���������λ
}






void gettempstr(void)
{   
    
    unsigned char temh,teml;
    teml = temp_data[0];
    temh = temp_data[1];
    ch[0] = ' ';
    ch[1] = ' ';
    
    if(temh & 0x80)            //�ж������¶�
    {
        ch[2]='-';              //���λΪ��
    }
    else ch[2]='+';
    if(temh/100==0)
        ch[3]=' ';
    else ch[3]=temh/100+0x30;      //+0x30 Ϊ�� 0~9 ASCII��
    if((temh/10%10==0)&&(temh/100==0))
        ch[4]=' ';
    else ch[4]=temh/10%10+0x30;
    ch[5]=temh%10+0x30;
    ch[6]='.';
    ch[7]=teml+0x30;// С������
}