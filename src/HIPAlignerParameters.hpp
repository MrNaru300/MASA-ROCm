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
#include "libmasa/libmasa.hpp"


/**
 * Constant used to select fastest GPU available.
 */
#define DETECT_FASTEST_GPU (-1)

/**
 * Parameters for the MASA-ROCm extension.
 */
class HIPAlignerParameters : public AbstractAlignerParameters {
private:
	/** Selected GPU or DETECT_FASTEST_GPU for automatic selection */
	int gpu;

	/** Fixed number of blocks or 0 (zero) for auto configuration */
	int blocks;

public:
	HIPAlignerParameters();
	virtual ~HIPAlignerParameters();

	virtual int processArgument(int argc, char** argv);
	virtual void printUsage() const;

	int getBlocks() const;
	void setBlocks(int blocks);
	int getGPU() const;
	void setGPU(int gpu);

};

