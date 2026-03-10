#ifndef __KALMAN_H
#define __KALMAN_H

#include "stm32f10x.h"

// 一维卡尔曼滤波器结构体
typedef struct {
    float LastP; // 上一次估算误差协方差
    float out;   // 滤波器输出值
    float Q;     // 过程噪声协方差 (越小越平滑，但响应越慢)
    float R;     // 测量噪声协方差 (越大越平滑，但响应越慢)
    uint8_t is_init; // 用于标记是否已初始化，防止开机数据从0缓慢爬升
} Kalman_TypeDef;

// 初始化滤波器
static inline void Kalman_Init(Kalman_TypeDef *kf, float Q, float R) {
    kf->LastP = 1.0f;
    kf->out = 0.0f;
    kf->Q = Q;
    kf->R = R;
    kf->is_init = 0;
}

// 滤波器更新核心算法
static inline float Kalman_Filter(Kalman_TypeDef *kf, float input) {
    // 首次传入真实数据时，直接作为初始状态
    if (!kf->is_init) {
        kf->out = input;
        kf->is_init = 1;
    }
    
    // 1. 预测协方差
    float Now_P = kf->LastP + kf->Q;
    
    // 2. 计算卡尔曼增益
    float Kg = Now_P / (Now_P + kf->R);
    
    // 3. 更新输出估计值
    kf->out = kf->out + Kg * (input - kf->out);
    
    // 4. 更新协方差
    kf->LastP = (1.0f - Kg) * Now_P;
    
    return kf->out;
}

#endif
