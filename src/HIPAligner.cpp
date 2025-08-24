/*******************************************************************************
 *
 * Copyright (c) 2010-2015   Edans Sandes
 *
 * This file is part of MASA-CUDAlign.
 * 
 * MASA-CUDAlign is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * MASA-CUDAlign is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MASA-CUDAlign.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "HIPAligner.hpp"
#include "hip_util.h"
#include "config.h"

#include <stdio.h>
#include <getopt.h>


/**
 * Set to (1) in order to print debug information in the stdout. This
 * significantly degrades the performance.
 */
#define DEBUG (0)

/**
 * Defines the maximum number of forks for multigpu processes
 */
#define MAX_GPUS		(8)

//TODO: mudar para tamanho dinâmico
/**
 * Maximum sequence size considering the 2^27 CUDA texture limit.
 */
#define MAX_SEQUENCE_SIZE (134150000)


/* ************************************************************************** */
/*                             Constructors                                   */
/* ************************************************************************** */

/**
 * Instantiates the HIPAligner object.
 */
HIPAligner::HIPAligner() {
	/* Hard-coded alignment parameters */
	score_params.match = DNA_MATCH;
	score_params.mismatch = DNA_MISMATCH;
	score_params.gap_open = DNA_GAP_OPEN;
	score_params.gap_ext = DNA_GAP_EXT;

	params = new HIPAlignerParameters();

	/* Obtains the GPU weights for multiprocess forked instances */
	int weight[MAX_GPUS];
	int count = getGPUWeights(weight, MAX_GPUS);
	setForkCount(count, weight);
}

/**
 * Destroys the HIPAligner object.
 */
HIPAligner::~HIPAligner() {

}


/* ************************************************************************** */
/*                             Virtual methods                                */
/* ************************************************************************** */

/**
 * Returns the capability of the aligner.
 *
 * @return the aligner_capabilities_t object containing the capabilities of
 * 	this Aligner.
 */
aligner_capabilities_t HIPAligner::getCapabilities() {
	aligner_capabilities_t capabilities;
	capabilities.smith_waterman 			= SUPPORTED;
	capabilities.needleman_wunsch 			= SUPPORTED;
	capabilities.block_pruning 				= SUPPORTED;
	capabilities.customize_first_column 	= SUPPORTED;
	capabilities.customize_first_row 		= SUPPORTED;
	capabilities.dispatch_last_cell 		= SUPPORTED;
	capabilities.dispatch_last_column 		= SUPPORTED;
	capabilities.dispatch_last_row 			= SUPPORTED;
	capabilities.dispatch_special_column 	= NOT_SUPPORTED;
	capabilities.dispatch_special_row 		= SUPPORTED;
	capabilities.dispatch_block_scores		= SUPPORTED;
	capabilities.dispatch_scores			= SUPPORTED;
	capabilities.process_partition 			= SUPPORTED;
	capabilities.variable_penalties 		= NOT_SUPPORTED;
	capabilities.fork_processes				= SUPPORTED;



	capabilities.maximum_seq0_len	= MAX_SEQUENCE_SIZE;
	capabilities.maximum_seq1_len	= MAX_SEQUENCE_SIZE;
	//capabilities.maximum_seq0_len	= 500000;
	//capabilities.maximum_seq1_len	= 500000;

	return capabilities;
}

/**
 * Returns the Custom Parameters of this Aligner.
 * @return the HIPAlignerParameters associated with this aligner.
 */
AbstractAlignerParameters* HIPAligner::getParameters() {
	return this->params;
}

/**
 * Returns the SW score parameters. Currently, HIPAlign uses only fixed parameters.
 * @return the object containing the score parameters.
 */
const score_params_t* HIPAligner::getScoreParameters() {
	return &score_params;
}

/**
 * Initialize the aligner. This method is called only once during the
 * aligner lifetime. Here we already have the HIPAlignerParameters
 * completely set and, then, we initialize the selected GPU and allocated
 * the fixed structures that will be used during all the execution.
 *
 * @see IAligner::initialize()
 */
void HIPAligner::initialize() {
	if (this->params->getForkId() != NOT_FORKED_INSTANCE) {
		/* Selects a different GPU for each process */
		int ids[MAX_GPUS];
		int gpus = getAvailableGPU(ids, MAX_GPUS);
		int gpuID = this->params->getForkId();
		if (gpuID >= gpus) {
			gpuID = gpuID % gpus;
			fprintf(stderr, "INFO: Wrapping gpu ID (%d -> %d). (max.: %d).\n", this->params->getForkId(), gpuID, gpus);
		}
		fprintf(stderr, "DEBUG: %d -> %d.\n", this->params->getForkId(), ids[gpuID]);
		this->params->setGPU(ids[gpuID]);
	}
	selectGPU(this->params->getGPU());

	/* Checks HIP capability */
	if (getCompiledCapability() > getDevCapability()) {
			fprintf(stderr, "FATAL: Unsupported GPU Device Capability. (%d < %d).\n", getDevCapability(), getCompiledCapability());
			fprintf(stderr, "Maybe you should try to recompile the code with an old hip architecture.\n");
			exit(1);
	}

	multiprocessors = getGPUMultiprocessors();

	// Alocates the default structures.
	allocateGlobalStructures();

}

/**
 * Finalizes the execution of the Smith-Waterman.
 */
void HIPAligner::finalize() {
	freeGlobalStructures();

	hipDeviceReset();
	hipUtilCheckMsg("hipDeviceReset failed");
}

/**
 * Initializes structures before processing diagonals.
 */
void HIPAligner::initializeDiagonals() {
	int blocks = getGrid()->getGridWidth();

	/* Defines the proper grid division */
	copy_split(getGrid()->getBlockSplitHorizontal(), blocks);

	/* Clear previous block results */
	for (int i=0; i<blocks; i++) {
		host.h_blockResult[i].x = -INF;
		host.h_blockResult[i].y = -INF;
		host.h_blockResult[i].z = -INF;
		host.h_blockResult[i].w = -INF;
	}
	hipUtilSafeCall(hipMemcpy(hip.d_blockResult, host.h_blockResult, sizeof(int4)*blocks, hipMemcpyHostToDevice));
}

/**
 * Processes one external diagonal.
 *
 * @param diagonal diagonal number.
 * @param windowLeft pruning window left.
 * @param windowRight pruning window right.
 */
void HIPAligner::processDiagonal(int diagonal, int windowLeft, int windowRight) {
	int2 cutBlock;
	cutBlock = make_int2(windowLeft, windowRight);

	lauch_external_diagonals(getFirstColumnInitType(), mustDispatchLastColumn(),
			getRecurrenceType(), mustDispatchScores(),
			getGrid()->getGridWidth(), getThreadCount(), getPartition().getI0(), getPartition().getI1(), diagonal, cutBlock, &hip);
}

/**
 * Finalizes structures after processing diagonals.
 */
void HIPAligner::finalizeDiagonals() {
	// Does nothing
}

/**
 * Defines the correct direction of the sequences an limits the
 * range of the sequences that will be processed. The memory allocation
 * in GPU is done accordingly to the length of the sequences.
 *
 * @param seq0	sequence#1 (vertical)
 * @param seq1	sequence#2 (horizontal)
 * @param seq0_len	vertical sequence length.
 * @param seq1_len	horizontal sequence length.
 * @see IAligner::setSequences
 */
void HIPAligner::setSequences(
		const char* seq0, const char* seq1, int seq0_len, int seq1_len) {
	if (DEBUG) printf("HIPAligner::allocateStructures(%d,%d)\n", seq0_len, seq1_len);
	if (seq0_len == 0 || seq1_len == 0) {
		// TODO ensure in the AlignerManager that the onSequenceChange will
		// only be executed if the sequences are not zero-length
		return;
	}

	if (seq0_len > MAX_SEQUENCE_SIZE || seq1_len > MAX_SEQUENCE_SIZE) {
			fprintf(stderr, "FATAL: Sequence size is too big. (limit.: %d BP).\n", MAX_SEQUENCE_SIZE);
			fprintf(stderr, "Split to smaller sequences.\n");
			exit(1);
	}

	size_t usedMemoryBefore;
	getMemoryUsage(&usedMemoryBefore);

	const int seq0_padding = MAX_GRID_HEIGHT;
	const int seq1_padding = 0;
	const int bus_size = (seq1_len+seq1_padding+1)*sizeof(int2); // extra padding is for first read at opening

	host.h_busH = (int2* )malloc(bus_size);

	hip.d_seq0 = allocHipSeq(seq0, seq0_len, seq0_padding, '\0');
	hip.d_seq1 = allocHipSeq(seq1, seq1_len, seq1_padding, '\0');
	hip.busH_size = bus_size;

	bind_textures(hip.d_seq0, seq0_len+seq0_padding, hip.d_seq1, seq1_len+seq1_padding);

	hip.d_busH         = (int2*)  allocHip0(bus_size);

    size_t usedMemory;
	getMemoryUsage(&usedMemory);
	statVariableAllocatedMemory = usedMemory - usedMemoryBefore;
	statDeallocatedMemory = usedMemory - statInitialUsedMemory;
}

/**
 * Dispose all structures related to the sequence sizes.
 */
void HIPAligner::unsetSequences() {
	if (DEBUG) printf("HIPAligner::freeStructures()\n");

	free(host.h_busH);
	host.h_busH = NULL;

	hipUtilSafeCall(hipFree(hip.d_seq0));
	hipUtilSafeCall(hipFree(hip.d_seq1));
	hip.d_seq0 = NULL;
	hip.d_seq1 = NULL;
	unbind_textures();

	hipUtilSafeCall(hipFree(hip.d_busH));
	hip.d_busH = NULL;

	size_t usedMemory;
	getMemoryUsage(&usedMemory);
	statDeallocatedMemory = usedMemory - statInitialUsedMemory;
}

/**
 * Returns the height of the block (number of cells). This is the number of threads in a block
 * multiplied by ALPHA (4).
 * @return the height of the blocks.
 */
int HIPAligner::getBlockHeight() {
	return getThreadCount()*ALPHA;
}

/**
 * Calculates the GPU block count for a given partition width (i.e.
 * the number of blocks in the grid width).
 *
 * @param width partition's width
 * @return number of GPU blocks. It is always less than MAX_BLOCKS and
 * than HIPAlignerParameters::blocks.
 */
int HIPAligner::getGridWidth(const int width) {
	int blocks = params->getBlocks();
	const int recommended = 4 * multiprocessors; // recommended number of simultaneous blocks per MP
	int maximum; // maximum recommended number of blocks per MP
	if (mustPruneBlocks()) {
		maximum = 1000 * recommended; // 1000 = don't care about maximum
	} else {
		maximum = recommended; // wider block is better
	}
	if (blocks == 0 || width < (2 * blocks * THREADS_COUNT)) {
		// Here we ensure the minimum size requirement
		blocks = width / 2 / THREADS_COUNT;

		if (blocks <= 1) {
			// This is the minimum number of blocks
			blocks = 1;
		} else {
			if (blocks > MAX_BLOCKS_COUNT) {
				// This is the maximum allowed number of blocks
				blocks = MAX_BLOCKS_COUNT;
			}

			if (blocks <= multiprocessors) {
				// DON'T delete this "if", otherwise
				//       the semantic will be incorrect.
			} else if (blocks <= maximum) {
				// round in multiples of multiprocessors
				blocks = (blocks / multiprocessors) * multiprocessors;
			} else {
				// ensure the maximum size
				blocks = maximum;
			}
		}
	}
	if (DEBUG) {
		printf("SIZES partition.width: %d  block.size: (%d,4x%d) B: %d %s\n",
				width, width / blocks, THREADS_COUNT, blocks,
				blocks == 1 ? "[ SMALL ]" : "");
	}
	return blocks;
}

/**
 * Returns the range of cells [j,j+len) from the last row of the matrix.
 *
 * @param j index of the first cell to be loaded.
 * @param len number of cells to be loaded.
 */
cell_t* HIPAligner::getLastRow(int j, int len) {
    hipError_t err = hipStreamSynchronize(0);
	if (err != hipSuccess) {
		fprintf(stderr, "Error synchronizing stream: %s\n", hipGetErrorString(err));
		return nullptr;
	}
    int j0 = getPartition().getJ0();

	/*
	 * The last row in GPU is shifted THREADS_COUNT bytes to the left in
	 * order to prevent the overwrite of the previous special rows (i.e
	 * since the busH structure is shared by all the blocks, the
	 * bottom-most blocks may overwrite the result of the other blocks).
	 * Since the busH may not be written in negative index,
	 * the extraH vector is used to store this out-of-bound cells.
	 * The remaining cells are written directly to the busH.
	 */
	const int shift = THREADS_COUNT;
	if (j-shift < j0) {
		int diff = j0 - (j-shift);
		if (diff > len) diff = len;
		if (DEBUG) printf("EXTRA x0: %d  diff: %d   xLen: %d!  mem: %p\n", j, diff, len, hip.d_extraH);
	    hipUtilSafeCall(hipMemcpy(host.h_busH+j, hip.d_extraH,
	    		(diff)*sizeof(int2), hipMemcpyDeviceToHost));
	    if (len-diff > 0) {
	    	hipUtilSafeCall(hipMemcpy(host.h_busH+j+diff, hip.d_busH+j0,
	    			(len-diff)*sizeof(int2), hipMemcpyDeviceToHost));
	    }
	} else {
	    hipUtilSafeCall(hipMemcpy(host.h_busH+j, hip.d_busH+j-shift,
	    		(len)*sizeof(int2), hipMemcpyDeviceToHost));
	}
	return (cell_t*)(host.h_busH + j);
}

/**
 * Returns the range of cells [j,j+len) from the special row that
 * crossed the previously computed diagonal.
 *
 * @param j index of the first cell to be loaded.
 * @param len number of cells to be loaded.
 */
cell_t* HIPAligner::getSpecialRow(int j, int len) {
	hipError_t err = hipStreamSynchronize(0);
	if (err != hipSuccess) {
		fprintf(stderr, "Error synchronizing stream: %s\n", hipGetErrorString(err));
		return nullptr;
	}
	hipUtilSafeCall(hipMemcpy(host.h_busH + j, hip.d_busH + j, len * sizeof(int2),
					hipMemcpyDeviceToHost));
	
	err = hipStreamSynchronize(0);
	if (err != hipSuccess) {
		fprintf(stderr, "Error synchronizing stream: %s\n", hipGetErrorString(err));
		return nullptr;
	}
	
	return (cell_t*)(host.h_busH + j);
}

/**
 * Returns the range of cells [i,i+len) from the last column of the matrix.
 *
 * @param i index of the first cell to be loaded.
 * @param len number of cells to be loaded.
 */
cell_t* HIPAligner::getLastColumn(int i, int len) {
	// unused parameters.

	int _len = THREADS_COUNT;
	hipUtilSafeCall(hipMemcpy(host.h_flushColumnH, hip.d_flushColumnH, _len*sizeof(int4), hipMemcpyDeviceToHost));
	hipUtilSafeCall(hipMemcpy(host.h_flushColumnE, hip.d_flushColumnE, _len*sizeof(int4), hipMemcpyDeviceToHost));

	/*
	 * We will interleave the H and E components into a vector, considering
	 * the 4-pack of cells.
	 *
	 * example:
	 * h_flushColumnH={{H1,H2,H3,H4}, {H5,H6,H7,H8}, ...}
	 * h_flushColumnE={{E1,E2,E3,E4}, {E5,E6,E7,E8}, ...}
	 *
	 * h_flushColumnTemp={{H1,E1},{H2,E2},{H3,E3},{H4,E4},{H5,E5},{H6,E6},...}
	 */
	for (int k = 0; k < _len; k++) {
		host.h_flushColumn[k * ALPHA + 0].h = host.h_flushColumnH[k].x;
		host.h_flushColumn[k * ALPHA + 1].h = host.h_flushColumnH[k].y;
		host.h_flushColumn[k * ALPHA + 2].h = host.h_flushColumnH[k].z;
		host.h_flushColumn[k * ALPHA + 3].h = host.h_flushColumnH[k].w;

		host.h_flushColumn[k * ALPHA + 0].e = host.h_flushColumnE[k].x;
		host.h_flushColumn[k * ALPHA + 1].e = host.h_flushColumnE[k].y;
		host.h_flushColumn[k * ALPHA + 2].e = host.h_flushColumnE[k].z;
		host.h_flushColumn[k * ALPHA + 3].e = host.h_flushColumnE[k].w;
	}
	return host.h_flushColumn;
}

/**
 * Returns a vector from the GPU containing the best score of each block.
 */
score_t* HIPAligner::getBlockScores() {
	int blocks = getGrid()->getGridWidth();
	hipUtilSafeCall(hipMemcpy(host.h_blockResult, hip.d_blockResult, sizeof(int4)*blocks, hipMemcpyDeviceToHost));
	for (int k=0; k<blocks; k++) {
		host.h_blockScores[k].i = host.h_blockResult[k].y;
		host.h_blockScores[k].j = host.h_blockResult[k].x;
		host.h_blockScores[k].score = host.h_blockResult[k].w;
	}


	return host.h_blockScores;
}

/**
 * Updates the first row of the matrix with the given cells.
 *
 * @param cells vector with the cells of the first row.
 * @param j column where the vector starts.
 * @param len length of the vector.
 */
void HIPAligner::setFirstRow(const cell_t* cells, int j, int len) {
	if (DEBUG) fprintf(stderr, "HIPAligner::setFirstRow(..., %d, %d)\n", j, len);
	hipUtilSafeCall(hipMemcpy(hip.d_busH + j, cells,
			len*sizeof(int2), hipMemcpyHostToDevice));
}

/**
 * Updates the first column of the matrix with the given cells.
 *
 * @param cells vector with the cells of the first column.
 * @param i row where the vector starts.
 * @param len length of the vector.
 */
void HIPAligner::setFirstColumn(const cell_t* cells, int i, int len) {
	/* Repeat previous diagonal cell */
    host.h_loadColumnH[0].w = cells[0].h;
    host.h_loadColumnE[0].w = cells[0].e;

	/*
	 * We will de-interleave the H and E components from the vector in
	 * packs of 4 cells.
	 *
	 * example:
	 * h_loadColumnTemp={{H1,E1},{H2,E2},{H3,E3},{H4,E4},{H5,E5},{H6,E6},...}
	 *
	 * h_loadColumnH={{H1,H2,H3,H4}, {H5,H6,H7,H8}, ...}
	 * h_loadColumnE={{E1,E2,E3,E4}, {E5,E6,E7,E8}, ...}
	 */
	for (int k = 1; k <= getThreadCount(); k++) {
		host.h_loadColumnH[k].x = cells[k * ALPHA - 3].h;
		host.h_loadColumnH[k].y = cells[k * ALPHA - 2].h;
		host.h_loadColumnH[k].z = cells[k * ALPHA - 1].h;
		host.h_loadColumnH[k].w = cells[k * ALPHA - 0].h;

		host.h_loadColumnE[k].x = cells[k * ALPHA - 3].e;
		host.h_loadColumnE[k].y = cells[k * ALPHA - 2].e;
		host.h_loadColumnE[k].z = cells[k * ALPHA - 1].e;
		host.h_loadColumnE[k].w = cells[k * ALPHA - 0].e;
	}
	hipUtilSafeCall(hipMemcpy(hip.d_loadColumnH, host.h_loadColumnH,
					(getBlockHeight() + 4) * sizeof(int), hipMemcpyHostToDevice));
	hipUtilSafeCall(hipMemcpy(hip.d_loadColumnE, host.h_loadColumnE,
					(getBlockHeight() + 4) * sizeof(int), hipMemcpyHostToDevice));
}

/**
 * Clears the bus for blocks [b0, b1).
 * @param b0 first block to be cleaned (inclusive).
 * @param b1 last block be cleaned (exclusive).
 */
void HIPAligner::clearPrunedBlocks(int b0, int b1) {
	int p0;
	int p1;
	getGrid()->getBlockPosition(b0, 0, NULL, &p0, NULL, NULL);
	getGrid()->getBlockPosition(b1, 0, NULL, &p1, NULL, NULL);
	initializeBusHInfinity(p0, p1, hip.d_busH);
}


/* ************************************************************************** */
/*                             Statistics methods                             */
/* ************************************************************************** */

/**
 * Clear the internal statistics.
 */
void HIPAligner::clearStatistics() {
	AbstractDiagonalAligner::clearStatistics();
}

/**
 * MASA-Core call this method to print some statistics after initialization.
 * @param file handler to print out the statistics.
 */
void HIPAligner::printInitialStatistics(FILE* file) {
	AbstractDiagonalAligner::printInitialStatistics(file);

	fprintf(file, "\n===== GPU DEVICE INFO  =====\n");
	printDevProp(file);

	fprintf(file, "\n===== HIP COMPILER PARAMETERS  =====\n");
	fprintf(file, " MAX BLOCKS: %d\n", MAX_BLOCKS_COUNT);
	fprintf(file, "    THREADS: %d\n", THREADS_COUNT);
	fprintf(file, "      ALPHA: %d\n", ALPHA);
	fprintf(file, "  HIP ARCH: %.1f\n", getCompiledCapability()/100.0f);
	fprintf(file, "  MAX_SEQUENCE_SIZE: %d\n", MAX_SEQUENCE_SIZE);

	fprintf(file, "\n===== GPU MEMORY USAGE =====\n");
	fprintf(file, " Total Global Memory: %lu MB\n", (unsigned long) (statTotalMem/1024/1024));
	fprintf(file, " Initial Used Memory: %lu KB\n", (unsigned long) (statInitialUsedMemory/1024));
	fprintf(file, "    Allocated Memory:+%lu/+%lu KB\n", (unsigned long) ((statFixedAllocatedMemory)/1024), (unsigned long) ((statVariableAllocatedMemory)/1024));
	fprintf(file, "  Deallocated Memory: %lu KB\n", (unsigned long) ((statDeallocatedMemory))/1024);

	fprintf(file, "\n===== RUNTIME PARAMETERS =====\n");
	fprintf(file, " Blocks: %d %s\n", params->getBlocks(), params->getBlocks()==0?"(AUTO)":"");
	fprintf(file, " ForkID: %d %s\n", params->getForkId(), params->getForkId()==NOT_FORKED_INSTANCE?"(NOT FORKED)":"");
	fprintf(file, "    GPU: %d %s\n", params->getGPU(), params->getGPU()==DETECT_FASTEST_GPU?"(AUTO)":"");

	fflush(file);
}

/**
 * MASA-Core call this method to print some statistics in the start of a stage.
 * @param file handler to print out the statistics.
 */
void HIPAligner::printStageStatistics(FILE* file) {
	AbstractDiagonalAligner::printStageStatistics(file);

	fprintf(file, "\n========== GPU MEMORY USAGE ==========\n");
	fprintf(file, "    Allocated Memory:+%lu/+%lu KB\n", (unsigned long) ((statFixedAllocatedMemory)/1024), (unsigned long) ((statVariableAllocatedMemory)/1024));
	fprintf(file, "  Deallocated Memory: %lu KB\n", (unsigned long) ((statDeallocatedMemory))/1024);
	fflush(file);
}

/**
 * MASA-Core call this method to print some statistics after finalization.
 * @param file handler to print out the statistics.
 */
void HIPAligner::printFinalStatistics(FILE* file) {
	AbstractDiagonalAligner::printFinalStatistics(file);

	fprintf(file, "\n========== FINAL MEMORY USAGE ==========\n");
	fprintf(file, "    Allocated Memory:+%lu/+%lu KB\n", (unsigned long) ((statFixedAllocatedMemory)/1024), (unsigned long) ((statVariableAllocatedMemory)/1024));
	fprintf(file, "  Deallocated Memory: %lu KB %s\n", (unsigned long) ((statDeallocatedMemory))/1024, statDeallocatedMemory==0?"(FINE)":"(LEAKED MEMORY!)");
	fflush(file);
}

/**
 * MASA-Core prints this statistics to show internal statistics of the Aligner.
 * @param file handler to print out the statistics.
 */
void HIPAligner::printStatistics(FILE* file) {
	AbstractDiagonalAligner::printStatistics(file);

	fprintf(file, "\n===== GPU MEMORY USAGE =====\n");
	fprintf(file, "   Deallocated Memory: %lu KB\n", (unsigned long) ((statDeallocatedMemory))/1024);

	fflush(file);
}


/* ************************************************************************** */
/*                             Private methods                                */
/* ************************************************************************** */


/**
 * Allocates the fixed structures for all the executions. This method is called
 * only once during the lifetime of the extension.
 */
void HIPAligner::allocateGlobalStructures() {
	getMemoryUsage(&statInitialUsedMemory, &statTotalMem);

	host.h_extraH          = (int2* )  malloc(THREADS_COUNT*sizeof(int2));
	host.h_flushColumnH    = (int4* )  malloc(THREADS_COUNT*sizeof(int4));
	host.h_flushColumnE    = (int4* )  malloc(THREADS_COUNT*sizeof(int4));
	host.h_flushColumn     = (cell_t*) malloc(ALPHA*THREADS_COUNT*sizeof(int2));
	host.h_loadColumnH     = (int4* )  malloc((THREADS_COUNT + 1)*sizeof(int4));
	host.h_loadColumnE     = (int4* )  malloc((THREADS_COUNT + 1)*sizeof(int4));
	host.h_blockResult     = (int4* )  malloc(MAX_BLOCKS_COUNT*sizeof(int4));
	host.h_blockScores     = (score_t*)malloc(MAX_BLOCKS_COUNT*sizeof(score_t));
	host.h_busH            = NULL;

	hip.d_extraH       = (int2*) allocHip0(THREADS_COUNT*sizeof(int2));
	hip.d_flushColumnH = (int4*) allocHip0(THREADS_COUNT*sizeof(int4));
	hip.d_flushColumnE = (int4*) allocHip0(THREADS_COUNT*sizeof(int4));
	hip.d_loadColumnH  = (int4*) allocHip0((THREADS_COUNT+1)*sizeof(int4));
	hip.d_loadColumnE  = (int4*) allocHip0((THREADS_COUNT+1)*sizeof(int4));
	hip.d_blockResult  = (int4*) allocHip0(MAX_BLOCKS_COUNT*sizeof(int4));
	hip.d_busV_h       = (int4*) allocHip0(MAX_GRID_HEIGHT*sizeof(int4));
	hip.d_busV_e       = (int4*) allocHip0(MAX_GRID_HEIGHT*sizeof(int4));
	hip.d_busV_o       = (int3*) allocHip0(MAX_GRID_HEIGHT*sizeof(int3));
	hip.d_seq0         = NULL;
	hip.d_seq1         = NULL;
	hip.d_busH         = NULL;

    size_t usedMemory;
	getMemoryUsage(&usedMemory);
	statFixedAllocatedMemory = usedMemory - statInitialUsedMemory;
	statDeallocatedMemory = statFixedAllocatedMemory;
}

/**
 * Deallocates the fixed structures.
 */
void HIPAligner::freeGlobalStructures() {
	if (DEBUG) printf("freeFixedSizeStructures()\n");
	free(host.h_extraH);
	free(host.h_flushColumnH);
	free(host.h_flushColumnE);
	free(host.h_flushColumn);
	free(host.h_loadColumnH);
	free(host.h_loadColumnE);
	free(host.h_blockResult);
	free(host.h_blockScores);

	hipUtilSafeCall(hipFree(hip.d_extraH));
	hipUtilSafeCall(hipFree(hip.d_flushColumnH));
	hipUtilSafeCall(hipFree(hip.d_flushColumnE));
	hipUtilSafeCall(hipFree(hip.d_loadColumnH));
	hipUtilSafeCall(hipFree(hip.d_loadColumnE));
	hipUtilSafeCall(hipFree(hip.d_blockResult));
	hipUtilSafeCall(hipFree(hip.d_busV_h));
	hipUtilSafeCall(hipFree(hip.d_busV_e));
	hipUtilSafeCall(hipFree(hip.d_busV_o));

    size_t usedMemory;
	getMemoryUsage(&usedMemory);
	statDeallocatedMemory = usedMemory - statInitialUsedMemory;

}


/**
 * Returns the number of threads in each block. The default number
 * is defined in the THREADS_COUNT constant, but we cannot have more
 * threads then the partition width. We don't consider the bottom most
 * blocks, that may have less threads.
 *
 * @return the number of threads in each block.
 */
int HIPAligner::getThreadCount() {
	return getPartition().getWidth() <=THREADS_COUNT ? getPartition().getWidth() : THREADS_COUNT;
}
