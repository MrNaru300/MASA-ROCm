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


#include "HIPAlignerParameters.hpp"
#include "hip_util.h"

#include "HIPAligner.hpp"

#include <stdio.h>
#include <getopt.h>

#include <sstream>
using namespace std;

#define USAGE "\
--gpu=GPU           Selects the index of GPU used for the computation. If  \n\
                           GPU is not informed, the fastest GPU is selected.   \n\
                           A list of available GPUs can be obtained with the   \n\
                           --list-gpus parameter. \n\
--list-gpus             Lists all available GPUs. \n\
--blocks=B              Run B GPU Blocks\n\
"

#define ARG_GPU                 0x1001
#define ARG_LIST_GPUS           0x1002
#define ARG_BLOCKS              0x1003

/**
 * Optargs parameters
 */
static struct option long_options[] = {
        {"gpu",         required_argument,      0, ARG_GPU},
        {"list-gpus",   no_argument,            0, ARG_LIST_GPUS},
        {"blocks",      required_argument,      0, ARG_BLOCKS},
        {0, 0, 0, 0}
    };

/**
 * HIPAlignerParameters constructor.
 */
HIPAlignerParameters::HIPAlignerParameters() {
	blocks = 0;
	gpu = DETECT_FASTEST_GPU;
}

/**
 * HIPAlignerParameters destructor.
 */
HIPAlignerParameters::~HIPAlignerParameters(){
}

/*
 * See description in header file.
 */
void HIPAlignerParameters::printUsage() const {
	AbstractAlignerParameters::printFormattedUsage("HIP Specific Options", USAGE);
}

/**
 * Processes the argument for the HIPAlignerParameters;
 * @copydoc AbstractAlignerParameters::processArgument
 */
int HIPAlignerParameters::processArgument(int argc, char** argv) {
	int ret = AbstractAlignerParameters::callGetOpt(argc, argv, long_options);
	switch ( ret ) {
		case ARG_GPU:
			if (optarg != NULL) {
				sscanf(optarg, "%d", &gpu);
			}
			break;
		case ARG_LIST_GPUS:
			printGPUDevices();
			exit(1);
			break;
		case ARG_BLOCKS:
			if (optarg != NULL) {
				sscanf(optarg, "%d", &blocks);
				if (blocks > MAX_BLOCKS_COUNT) {
					stringstream out;
					out << "Blocks count cannot be greater than "
							<< MAX_BLOCKS_COUNT << ".";
					setLastError(out.str().c_str());
					return -1;
				}
			}
			break;
		default:
			return ret;
	}
	return 0;

}


int HIPAlignerParameters::getBlocks() const {
	return blocks;
}

void HIPAlignerParameters::setBlocks(int blocks) {
	this->blocks = blocks;
}

int HIPAlignerParameters::getGPU() const {
	return gpu;
}

void HIPAlignerParameters::setGPU(int gpu) {
	this->gpu = gpu;
}



