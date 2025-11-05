/*******************************************************************************
 *
 * Copyright (c) 2010-2015   Edans Sandes
 * Copyright (c) 2025        Bruno Santiago de Oliveira (Modifications for ROCm)
 *
 * This file is part of MASA-ROCm, based on MASA-CUDAlign.
 *
 * MASA-ROCm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MASA-ROCm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MASA-ROCm.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#pragma once

/**
 * @file hip_util.h
 * @brief File with basic functions and macros for HIP calls.
 */

#include <stdio.h>
#include <stdlib.h>

#include <hip/hip_runtime.h>


#define hipUtilSafeCall(err)           __hipSafeCall      (err, __FILE__, __LINE__)
inline void __hipSafeCall( hipError_t err, const char *file, const int line )
{
    if( hipSuccess != err) {
        fprintf(stderr, "%s(%i) : hipSafeCall() HIP Runtime API error : %s.\n",
                file, line, hipGetErrorString( err) );
        exit(-1);
    }
}

#define hipUtilCheckMsg(msg)           __hipCheckMsg     (msg, __FILE__, __LINE__)
inline void __hipCheckMsg( const char *errorMessage, const char *file, const int line )
{
    hipError_t err = hipGetLastError();
    if( hipSuccess != err) {
        fprintf(stderr, "%s(%i) : hipCheckMsg() HIP error : %s : %s.\n",
                file, line, errorMessage, hipGetErrorString( err) );
        exit(-1);
    }
#ifdef DEBUG
    err = hipDeviceSynchronize();
    if( hipSuccess != err) {
        fprintf(stderr, "%s(%i) : hipCheckMsg hipDeviceSynchronize error: %s : %s.\n",
                file, line, errorMessage, hipGetErrorString( err) );
        exit(-1);
    }
#endif
}

void* allocHip0(int size);
unsigned char* allocHipSeq(const char* data, const int len, const int padding_len=0, const char padding_char='\0');
void printDevProp(FILE* file=stdout);
void getMemoryUsage(size_t* freeMem, size_t* totalMem=NULL);
void printGPUDevices(FILE* file=stdout);
int getGPUMultiprocessors();
void selectGPU(int id);
int getGPUWeights(int* proportion, int n);
int getAvailableGPU(int* ids, int n);
int getDevCapability();
int getCompiledCapability();
