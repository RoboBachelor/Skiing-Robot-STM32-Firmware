/**
  ****************************(C) COPYRIGHT 2016 DJI****************************
  * @file       INSTask.c/h
  * @brief      ��Ҫ����������mpu6500��������ist8310�������̬���㣬�ó�ŷ���ǣ�
  *             �ṩͨ��mpu6500��data ready �ж�����ⲿ�������������ݵȴ��ӳ�
  *             ͨ��DMA��SPI�����ԼCPUʱ�䣬�ṩע�Ͷ�Ӧ�ĺ궨�壬�ر�DMA��
  *             DR���ⲿ�жϵķ�ʽ.
  * @note       SPI �������ǳ�ʼ����ʱ����Ҫ����2MHz��֮���ȡ���������20MHz
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Dec-26-2018     RM              1. ���
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2016 DJI****************************
  */

#include "INS_Task.h"

#include "stm32f4xx.h"


#include "spi.h"
#include "exit_init.h"
#include "IST8310driver.h"
#include "mpu6500driver.h"
#include "mpu6500reg.h"
#include "mpu6500driver_middleware.h"

#include "AHRS.h"

#include "pid.h"

#include "delay.h"
#include "pwm.h"
#include "buzzer.h"
#include "led.h"

#define IMUWarnBuzzerOn() buzzer_on(95, 10000) //����������У׼������

#define IMUWarnBuzzerOFF() buzzer_off() //����������У׼�������ر�

#define MPU6500_TEMPERATURE_PWM_INIT() TIM3_Init(MPU6500_TEMP_PWM_MAX, 1) //�������¶ȿ���PWM��ʼ��
#define IMUTempPWM(pwm) TIM_SetCompare2(TIM3, (pwm))                      //pwm����
#define INS_GET_CONTROL_TEMPERATURE() get_control_temperate()             //��ȡ�����¶ȵ�Ŀ��ֵ

#if defined(MPU6500_USE_DATA_READY_EXIT)

#define MPU6500_DATA_READY_EXIT_INIT() GPIOB_Exti8_GPIO_Init() //��ʼ��mpu6500�� �ⲿ�ж� ʹ��PB8 �ⲿ�ж��� 8

#define MPU6500_DATA_READY_EXIT_IRQHandler EXTI9_5_IRQHandler //�궨���ⲿ�жϺ�����ʹ����line8�ⲿ�ж�

#define MPU6500_DATA_READY_EXIT_Line EXTI_Line8 //�궨���ⲿ�ж���
#endif

#define IMU_BOARD_INSTALL_SPIN_MATRIX                           \
                                        { 1.0f, 0.0f, 0.0f},    \
                                        { 0.0f, 0.0f,-1.0f},    \
                                        { 0.0f, 1.0f, 0.0f}     

//���������ǣ����ٶȼƣ����������ݵ����Զȣ���Ư
static void IMU_Cali_Slove(fp32 gyro[3], fp32 accel[3], fp32 mag[3], mpu6500_real_data_t *mpu6500, ist8310_real_data_t *ist8310);
static void IMU_temp_Control(fp32 temp);

static uint8_t mpu6500_spi_rxbuf[DMA_RX_NUM]; //������յ�ԭʼ����
mpu6500_real_data_t mpu6500_real_data; //ת���ɹ��ʵ�λ��MPU6500����
static fp32 Gyro_Scale_Factor[3][3] = {IMU_BOARD_INSTALL_SPIN_MATRIX}; //������У׼���Զ�
 fp32 gyro_cali_offset[3] ={0.0f, 0.0f, 0.0f};
 fp32 Gyro_Offset[3] = {0.0f, 0.0f, 0.0f};            //��������Ư
static fp32 Accel_Scale_Factor[3][3] = {IMU_BOARD_INSTALL_SPIN_MATRIX}; //���ٶ�У׼���Զ�
static fp32 Accel_Offset[3] = {0.0f, 0.0f, 0.0f};            //���ٶ���Ư
ist8310_real_data_t ist8310_real_data;                //ת���ɹ��ʵ�λ��IST8310����
static fp32 Mag_Scale_Factor[3][3] = {{0.0f, 1.0f, 0.0f},
                                      {0.0f, 0.0f, 1.0f},
                                      {-1.0f, 0.0f, 0.0f}}; //������У׼���Զ�
static fp32 Mag_Offset[3] = {0.0f, 0.0f, 0.0f};            //��������Ư
static const float TimingTime = INS_DELTA_TICK * 0.001f;   //�������е�ʱ�� ��λ s

fp32 INS_gyro[3] = {0.0f, 0.0f, 0.0f};
fp32 INS_accel[3] = {0.0f, 0.0f, 0.0f};
fp32 INS_mag[3] = {0.0f, 0.0f, 0.0f};

fp32 INS_Angle[3] = {0.0f, 0.0f, 0.0f};      //ŷ���� ��λ rad
fp32 INS_quat[4] = {0.0f, 0.0f, 0.0f, 0.0f}; //��Ԫ��

static const fp32 imuTempPID[3] = {MPU6500_TEMPERATURE_PID_KP, MPU6500_TEMPERATURE_PID_KI, MPU6500_TEMPERATURE_PID_KD};
static PidTypeDef imuTempPid;

uint8_t first_temperate = 0;
uint8_t MPU6050_Data_Ready = 0;

float INS_GET_CONTROL_TEMPERATURE(){
	return 40.f;
}

void INS_Init()
{
		delay_ms(5);
    //��ʼ��mpu6500��ʧ�ܽ�����ѭ��
    while (mpu6500_init() != MPU6500_NO_ERROR)
    {
        ;
    }

//��ʼ��ist8310��ʧ�ܽ�����ѭ��
#if defined(USE_IST8310)
    while (ist8310_init() != IST8310_NO_ERROR)
    {
        ;
    }
#endif

    //��ʼ��mpu6500������׼�����ⲿ�ж�
    MPU6500_DATA_READY_EXIT_INIT();
}


void INS_Task(){
//�����ʹ��SPI�ķ�������ʹ����ͨSPIͨ�ŵķ���
#ifndef MPU6500_USE_SPI_DMA
        mpu6500_read_muli_reg(MPU_INT_STATUS, mpu6500_spi_rxbuf, DMA_RX_NUM);
#endif

        //����ȡ����mpu6500ԭʼ���ݴ���ɹ��ʵ�λ������
        mpu6500_read_over((mpu6500_spi_rxbuf + MPU6500_RX_BUF_DATA_OFFSET), &mpu6500_real_data);

//����ȡ����ist8310ԭʼ���ݴ���ɹ��ʵ�λ������
#if defined(USE_IST8310)
        ist8310_read_over((mpu6500_spi_rxbuf + IST8310_RX_BUF_DATA_OFFSET), &ist8310_real_data);
#endif
        //��ȥ��Ư�Լ���ת����ϵ
        IMU_Cali_Slove(INS_gyro, INS_accel, INS_mag, &mpu6500_real_data, &ist8310_real_data);


        //���ٶȼƵ�ͨ�˲�
        static fp32 accel_fliter_1[3] = {0.0f, 0.0f, 0.0f};
        static fp32 accel_fliter_2[3] = {0.0f, 0.0f, 0.0f};
        static fp32 accel_fliter_3[3] = {0.0f, 0.0f, 0.0f};
        static const fp32 fliter_num[3] = {1.929454039488895f, -0.93178349823448126f, 0.002329458745586203f};


        //�ж��Ƿ��һ�ν��룬�����һ�����ʼ����Ԫ����֮�������Ԫ������Ƕȵ�λrad
        static uint8_t updata_count = 0;

        if( mpu6500_real_data.status & 1 << MPU_DATA_READY_BIT)
        {

            if (updata_count == 0)
            {
                MPU6500_TEMPERATURE_PWM_INIT();
                PID_Init(&imuTempPid, PID_DELTA, imuTempPID, MPU6500_TEMPERATURE_PID_MAX_OUT, MPU6500_TEMPERATURE_PID_MAX_IOUT);

                //��ʼ����Ԫ��
                AHRS_init(INS_quat, INS_accel, INS_mag);
                get_angle(INS_quat, INS_Angle, INS_Angle + 1, INS_Angle + 2);

                accel_fliter_1[0] = accel_fliter_2[0] = accel_fliter_3[0] = INS_accel[0];
                accel_fliter_1[1] = accel_fliter_2[1] = accel_fliter_3[1] = INS_accel[1];
                accel_fliter_1[2] = accel_fliter_2[2] = accel_fliter_3[2] = INS_accel[2];
                updata_count++;
            }
            else
            {
                //���ٶȼƵ�ͨ�˲�
                accel_fliter_1[0] = accel_fliter_2[0];
                accel_fliter_2[0] = accel_fliter_3[0];

                accel_fliter_3[0] = accel_fliter_2[0] * fliter_num[0] + accel_fliter_1[0] * fliter_num[1] + INS_accel[0] * fliter_num[2];

                accel_fliter_1[1] = accel_fliter_2[1];
                accel_fliter_2[1] = accel_fliter_3[1];

                accel_fliter_3[1] = accel_fliter_2[1] * fliter_num[0] + accel_fliter_1[1] * fliter_num[1] + INS_accel[1] * fliter_num[2];

                accel_fliter_1[2] = accel_fliter_2[2];
                accel_fliter_2[2] = accel_fliter_3[2];

                accel_fliter_3[2] = accel_fliter_2[2] * fliter_num[0] + accel_fliter_1[2] * fliter_num[1] + INS_accel[2] * fliter_num[2];

                //������Ԫ��
                AHRS_update(INS_quat, TimingTime, INS_gyro, accel_fliter_3, INS_mag);
                get_angle(INS_quat, INS_Angle, INS_Angle + 1, INS_Angle + 2);


							
                //�����ǿ���У׼
                {
                    static int16_t start_gyro_cali_time = -1;
										if(first_temperate == 1){
												start_gyro_cali_time = 0;
												++first_temperate;
										}
                    if(start_gyro_cali_time == 0)
                    {
                        Gyro_Offset[0] = gyro_cali_offset[0];
                        Gyro_Offset[1] = gyro_cali_offset[1];
                        Gyro_Offset[2] = gyro_cali_offset[2];
                        start_gyro_cali_time++;
                    }
                    else if (start_gyro_cali_time < GYRO_OFFSET_START_TIME && start_gyro_cali_time > 0)
                    {
                        IMUWarnBuzzerOn();
												LED_R_On;
												//������gyro_offset������������˶�start_gyro_cali_time++��������˶� start_gyro_cali_time = 0
												gyro_offset(Gyro_Offset, INS_gyro, mpu6500_real_data.status, &start_gyro_cali_time);
                    }
                    else if (start_gyro_cali_time == GYRO_OFFSET_START_TIME)
                    {
                        IMUWarnBuzzerOFF();
												LED_R_Off;
                        start_gyro_cali_time++;
                    }
                }       //�����ǿ���У׼   code end

            }           //update count if   code end
        }               //mpu6500 status  if end
				
        //�¶ȿ��ƴ���
        IMU_temp_Control(mpu6500_real_data.temp);
}

/**
  * @brief          У׼������
  * @author         RM
  * @param[in]      �����ǵı������ӣ�1.0fΪĬ��ֵ�����޸�
  * @param[in]      �����ǵ���Ư���ɼ������ǵľ�ֹ�������Ϊoffset
  * @param[in]      �����ǵ�ʱ�̣�ÿ����gyro_offset���û��1,
  * @retval         ���ؿ�
  */
void INS_cali_gyro(fp32 cali_scale[3], fp32 cali_offset[3], int16_t *time_count)
{
    if (first_temperate)
    {
        if( *time_count == 0)
        {
            Gyro_Offset[0] = gyro_cali_offset[0];
            Gyro_Offset[1] = gyro_cali_offset[1];
            Gyro_Offset[2] = gyro_cali_offset[2];
        }
        gyro_offset(Gyro_Offset, INS_gyro, mpu6500_real_data.status, time_count);

        cali_offset[0] = Gyro_Offset[0];
        cali_offset[1] = Gyro_Offset[1];
        cali_offset[2] = Gyro_Offset[2];
    }
}

/**
  * @brief          У׼���������ã�����flash���������ط�����У׼ֵ
  * @author         RM
  * @param[in]      �����ǵı������ӣ�1.0fΪĬ��ֵ�����޸�
  * @param[in]      �����ǵ���Ư
  * @retval         ���ؿ�
  */
void INS_set_cali_gyro(fp32 cali_scale[3], fp32 cali_offset[3])
{
    gyro_cali_offset[0] = cali_offset[0];
    gyro_cali_offset[1] = cali_offset[1];
    gyro_cali_offset[2] = cali_offset[2];
}

const fp32 *get_INS_angle_point(void)
{
    return INS_Angle;
}
const fp32 *get_MPU6500_Gyro_Data_Point(void)
{
    return INS_gyro;
}

const fp32 *get_MPU6500_Accel_Data_Point(void)
{
    return INS_accel;
}

static void IMU_Cali_Slove(fp32 gyro[3], fp32 accel[3], fp32 mag[3], mpu6500_real_data_t *mpu6500, ist8310_real_data_t *ist8310)
{
    for (uint8_t i = 0; i < 3; i++)
    {
        gyro[i] = mpu6500->gyro[0] * Gyro_Scale_Factor[i][0] + mpu6500->gyro[1] * Gyro_Scale_Factor[i][1] + mpu6500->gyro[2] * Gyro_Scale_Factor[i][2] + Gyro_Offset[i];
        accel[i] = mpu6500->accel[0] * Accel_Scale_Factor[i][0] + mpu6500->accel[1] * Accel_Scale_Factor[i][1] + mpu6500->accel[2] * Accel_Scale_Factor[i][2] + Accel_Offset[i];
        mag[i] = ist8310->mag[0] * Mag_Scale_Factor[i][0] + ist8310->mag[1] * Mag_Scale_Factor[i][1] + ist8310->mag[2] * Mag_Scale_Factor[i][2] + Mag_Offset[i];
    }
}

static void IMU_temp_Control(fp32 temp)
{
    uint16_t tempPWM;
    static uint8_t temp_constant_time = 0 ;
    if (first_temperate)
    {
        PID_Calc(&imuTempPid, temp, INS_GET_CONTROL_TEMPERATURE());
        if (imuTempPid.out < 0.0f)
        {
            imuTempPid.out = 0.0f;
        }
        tempPWM = (uint16_t)imuTempPid.out;
        IMUTempPWM(tempPWM);
    }
    else
    {
        //��û�дﵽ���õ��¶ȣ�һֱ����ʼ���
        if (temp > INS_GET_CONTROL_TEMPERATURE())
        {
            temp_constant_time ++;
            if(temp_constant_time > 200)
            {
                //�ﵽ�����¶ȣ�������������Ϊһ������ʣ���������
                first_temperate = 1;
                imuTempPid.Iout = MPU6500_TEMP_PWM_MAX / 2.0f;

            }
        }
        IMUTempPWM(MPU6500_TEMP_PWM_MAX - 1);
    }
}

#if defined(MPU6500_USE_DATA_READY_EXIT)
void MPU6500_DATA_READY_EXIT_IRQHandler(void)
{
    if (EXTI_GetITStatus(MPU6500_DATA_READY_EXIT_Line) != RESET)
    {
        EXTI_ClearITPendingBit(MPU6500_DATA_READY_EXIT_Line);
				MPU6050_Data_Ready = 1;
    }
}
#endif
