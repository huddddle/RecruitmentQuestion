#ifndef __TURN_
#define __TURN_

#include "global.h"

// ============================================================================
// 主转向接口 - 推荐使用
// ============================================================================

/**
 * @brief 左转指定角度（非阻塞）
 * @param degree 转向角度，单位：度
 * @return 0 = 转向完成, 1 = 转向进行中
 */
void Left_Turn(float degree);

/**
 * @brief 右转指定角度（非阻塞）
 * @param degree 转向角度，单位：度
 * @return 0 = 转向完成, 1 = 转向进行中
 */
void Right_Turn(float degree);

/**
 * @brief 转向到指定的偏航角（非阻塞）
 * @param target_yaw 目标偏航角，范围 [0, 360)
 */
void TurnToYaw(float target_yaw);

/**
 * @brief 陀螺仪辅助直线行走
 * @param speed 行走速度，范围 [0, 1000]
 * @param dir 目标偏航角，用于保持方向
 */


// ============================================================================
// 兼容接口 - 保留以支持现有代码
// ============================================================================

/**
 * @brief 目标方向转向（已弃用，使用 TurnToYaw 替代）
 * @deprecated 使用 TurnToYaw() 替代
 */
void TurningRight(float Target_Dir);

/**
 * @brief 左转到目标方向（已弃用）
 * @deprecated 使用 TurnToYaw() 替代
 */
void TurningLeft(float target_dir);

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 限制角度在指定范围内
 * @param x 指向角度值的指针
 * @param range 范围（通常为 360 或 180）
 * @note [-range, range] 范围内的值保持不变
 */
void RangeLimite(float *x, float range);

/**
 * @brief 限制角度在 [-180, 180] 范围内（已弃用）
 * @deprecated 使用 RangeLimite() 替代
 */
void _360RangeLimite(float *x);

#endif