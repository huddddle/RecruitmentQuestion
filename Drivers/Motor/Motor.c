#include "Motor.h"

static float speed_fb = 0;
static float speed_spin = 0;
static int speed_L = 0;
static int speed_R = 0;

/******************************************************************
 * 函 数 名 称：TB6612_Motor_Stop
 * 函 数 说 明：A端和B端电机停止
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
******************************************************************/
void TB6612_Motor_Stop(void)
{
    AIN1_OUT(0);
    AIN2_OUT(0);
    BIN1_OUT(0);
    BIN2_OUT(0);
}

/******************************************************************
 * 函 数 名 称：AO_Control
 * 函 数 说 明：A端口电机控制
 * 函 数 形 参：dir旋转方向 1正转0反转   PWM旋转速度，范围（0 ~ per-1）
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：PWM 0-1000
******************************************************************/
void Right_Control(uint8_t dir, int32_t PWM)
{
  if (PWM > 800 || dir > 1) {
    PWM = 800;
  }
  if (PWM <= 0) 
  {
    PWM = 0;
  }

  if (dir == 1) {
    AIN1_OUT(1);
    AIN2_OUT(0);
  } else {
    AIN1_OUT(0);
    AIN2_OUT(1);
  }

    DL_TimerG_setCaptureCompareValue(PWM_0_INST, PWM, GPIO_PWM_0_C0_IDX);
}



/******************************************************************
 * 函 数 名 称：BO_Control
 * 函 数 说 明：B端口电机控制
 * 函 数 形 参：dir旋转方向 1正转0反转   PWM旋转速度，范围（0 ~ per-1）
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：speed 0-1000
******************************************************************/
void Left_Control(uint8_t dir, int32_t PWM)
{
  if (PWM > 800 || dir > 1) {
    PWM = 800;
  }
  if (PWM < 0) {
    PWM = 0;
  }

    if( dir == 1 )
    {
        BIN1_OUT(0);
        BIN2_OUT(1);
    }
    else
    {
        BIN1_OUT(1);
        BIN2_OUT(0);
    }

    DL_TimerG_setCaptureCompareValue(PWM_0_INST, PWM, GPIO_PWM_0_C1_IDX);
}

/******************************************************************
 * 函 数 名 称：motion_car_control
 * 函 数 说 明：电机偏离控制
 * 函 数 形 参：dir旋转方向 1正转0反转   PWM旋转速度，范围（0 ~ per-1）
 * 函 数 返 回：无
 * 作       者：
 * 备       注：PWM 0-1000
******************************************************************/
void motion_car_control(int16_t V_x, int16_t V_y, int16_t V_z)
{
    uint8_t A = 1, B = 1;
	float robot_APB = CAR_APB;
    speed_fb = V_x;
    speed_spin = (V_z / 1000.0f) * robot_APB;
    if (V_x == 0 && V_y == 0 && V_z == 0)
    {
        TB6612_Motor_Stop();
        return;
    }

    speed_L = speed_fb + speed_spin;
    speed_R= speed_fb  - speed_spin;

    if (speed_L > 999) speed_L = 999;
    if (speed_L < -999) speed_L = -999;

    if (speed_R > 999) speed_R = 999;
    if (speed_R < -999) speed_R = -999;

    if(speed_L < 0){
        A = 0;
        speed_L = -speed_L;
    }

    if(speed_R < 0){
        B = 0;
        speed_R = -speed_R;
    }

	Left_Control(A, speed_L);
    Right_Control(B, speed_R);

}