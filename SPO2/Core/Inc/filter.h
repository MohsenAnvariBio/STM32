/*
 * filter.h
 *
 *  Created on: Jan 29, 2022
 *      Author: Mario Regus
 *
 *  Note: Contains code adapted from Raivis Strogonivs
 *  https://morf.lv/implementing-pulse-oximeter-using-max30100
 *  https://github.com/xcoder123/MAX30100
 */

#ifndef __FILTER_H__
#define __FILTER_H__

#include "stm32f4xx_hal.h"

#define FS 230          // Sampling frequency

float process_ppg_signal(float ppg_signal_rdc, float *buffer, int M, int *i, int *filled);
float calculate_mean(float *buffer, int size);
void findPeaks(float *dataBuffer, int length, uint32_t *R, uint32_t *R_count);

#endif /* __FILTER_H__ */
