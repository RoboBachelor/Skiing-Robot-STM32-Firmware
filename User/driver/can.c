#include "stm32f4xx.h"
#include "can.h"
#include "main.h"

int16_t angle, speed;

/*
 * ��������CAN_GPIO_Config
 * ����  ��CAN��GPIO ����
 * ����  ����
 * ���  : ��
 * ����  ���ڲ�����
 */
static void CAN_GPIO_Config(void)
{
 	GPIO_InitTypeDef GPIO_InitStructure;
   	

  /* Enable GPIO clock */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	
	  /* Connect CAN pins to AF9 */
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF_CAN1);
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_CAN1);

	  /* Configure CAN RX & TX pins */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(GPIOD, &GPIO_InitStructure);
}

static void CAN_NVIC_Config(void)
{
   	NVIC_InitTypeDef NVIC_InitStructure;
		/* Configure one bit for preemption priority */
		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	 	/*�ж�����*/
		NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;	   //CAN RX0�ж�
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN1_NVIC;		   //��ռ���ȼ�0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;			   //�����ȼ�Ϊ0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static void CAN_Mode_Config(void)
{
	CAN_InitTypeDef        CAN_InitStructure;
	/************************CANͨ�Ų�������**********************************/
	/* Enable CAN clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

	/*CAN�Ĵ�����ʼ��*/
	CAN_DeInit(CAN1);
	CAN_StructInit(&CAN_InitStructure);

	/*CAN��Ԫ��ʼ��*/
	CAN_InitStructure.CAN_TTCM=DISABLE;			   //MCR-TTCM  �ر�ʱ�䴥��ͨ��ģʽʹ��
	CAN_InitStructure.CAN_ABOM=ENABLE;			   //MCR-ABOM  �Զ����߹��� 
	CAN_InitStructure.CAN_AWUM=ENABLE;			   //MCR-AWUM  ʹ���Զ�����ģʽ
	CAN_InitStructure.CAN_NART=DISABLE;			   //MCR-NART  ��ֹ�����Զ��ش�	  DISABLE-�Զ��ش�
	CAN_InitStructure.CAN_RFLM=DISABLE;			   //MCR-RFLM  ����FIFO ����ģʽ  DISABLE-���ʱ�±��ĻḲ��ԭ�б���  
	CAN_InitStructure.CAN_TXFP=DISABLE;			   //MCR-TXFP  ����FIFO���ȼ� DISABLE-���ȼ�ȡ���ڱ��ı�ʾ�� 
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;  //��������ģʽ
	CAN_InitStructure.CAN_SJW=CAN_SJW_1tq;		   //BTR-SJW ����ͬ����Ծ��� 2��ʱ�䵥Ԫ
	 
	/* ss=1 bs1=4 bs2=2 λʱ����Ϊ(1+4+2) �����ʼ�Ϊʱ������tq*(1+4+2)  */
	CAN_InitStructure.CAN_BS1=CAN_BS1_6tq;		   //BTR-TS1 ʱ���1 ռ����4��ʱ�䵥Ԫ
	CAN_InitStructure.CAN_BS2=CAN_BS2_2tq;		   //BTR-TS1 ʱ���2 ռ����2��ʱ�䵥Ԫ	
	
	/* CAN Baudrate = 1 MBps (1MBps��Ϊstm32��CAN�������) (CAN ʱ��Ƶ��Ϊ APB 1 = 42 MHz) */
	CAN_InitStructure.CAN_Prescaler = 5;		   ////BTR-BRP �����ʷ�Ƶ��  ������ʱ�䵥Ԫ��ʱ�䳤�� 42/(1+4+2)/6=1 Mbps
	CAN_Init(CAN1, &CAN_InitStructure);
}

/*
 * ��������CAN_Filter_Config
 * ����  ��CAN�Ĺ����� ����
 * ����  ����
 * ���  : ��
 * ����  ���ڲ�����
 */
static void CAN_Filter_Config(void)
{
	CAN_FilterInitTypeDef  CAN_FilterInitStructure;

	/*CANɸѡ����ʼ��*/
	CAN_FilterInitStructure.CAN_FilterNumber=0;						//ɸѡ����14
	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;	//����������ģʽ
	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit;	//ɸѡ��λ��Ϊ����32λ��
	/* ʹ��ɸѡ�������ձ�־�����ݽ��бȶ�ɸѡ����չID�������µľ����������ǵĻ��������FIFO0�� */

	CAN_FilterInitStructure.CAN_FilterIdHigh= 0x0000;		//Ҫɸѡ��ID��λ 
	CAN_FilterInitStructure.CAN_FilterIdLow= 0x0000; //Ҫɸѡ��ID��λ 
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh= 0x0000;			//ɸѡ����16λÿλ����ƥ��
	CAN_FilterInitStructure.CAN_FilterMaskIdLow= 0x0000;			//ɸѡ����16λÿλ����ƥ��
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0 ;				//ɸѡ����������FIFO0
	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE;			//ʹ��ɸѡ��
	CAN_FilterInit(&CAN_FilterInitStructure);
	/*CANͨ���ж�ʹ��*/
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
}

/*
 * ��������CAN_Config
 * ����  ����������CAN�Ĺ���
 * ����  ����
 * ���  : ��
 * ����  ���ⲿ����
 */
void CAN_Config(void)
{
  CAN_GPIO_Config();
  CAN_NVIC_Config();
  CAN_Mode_Config();
  CAN_Filter_Config();   
}

void CAN1_Send_Current(int16_t current1, int16_t current2, int16_t current3, int16_t current4){
	CanTxMsg TxMessage;
	TxMessage.StdId=0x200;
  TxMessage.IDE=CAN_ID_STD;					 //��չģʽ
  TxMessage.RTR=CAN_RTR_DATA;				 //���͵�������
  TxMessage.DLC=8;							 //���ݳ���Ϊ8�ֽ�
	TxMessage.Data[0] = (current1 >> 8) & 0xFF;
	TxMessage.Data[1] = (current1) & 0xFF;
	TxMessage.Data[2] = (current2 >> 8) & 0xFF;
	TxMessage.Data[3] = (current2) & 0xFF;
	TxMessage.Data[4] = (current3 >> 8) & 0xFF;
	TxMessage.Data[5] = (current3) & 0xFF;
	TxMessage.Data[6] = (current4 >> 8) & 0xFF;
	TxMessage.Data[7] = (current4) & 0xFF;
	CAN_Transmit(CAN1, &TxMessage);
}

void CAN1_RX0_IRQHandler(void)
{
	CanRxMsg RxMessage;
	/*�������ж�������*/
	CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);
	if(RxMessage.StdId == 0x203){
		angle = (RxMessage.Data[0] << 8) | (RxMessage.Data[1]);
		speed = (RxMessage.Data[2] << 8) | (RxMessage.Data[3]);
	}
}
