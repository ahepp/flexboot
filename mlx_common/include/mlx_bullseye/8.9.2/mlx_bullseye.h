/*
 * mlx_bullseye.h
 *
 *  Created on: Dec 16, 2014
 *      Author: moshikos
 */

#ifndef SRC_BULLSEYE_MLX_BULLSEYE_H_
#define SRC_BULLSEYE_MLX_BULLSEYE_H_


#ifdef MLX_BULLSEYE

#define MlxBullseyeDump() \
	printf ( "\n\n"\
				"############################################################\n"\
				"MlxBullseyeDump Start\n"\
				"############################################################\n"\
				);\
	cov_dumpData();\
	printf ( "\n\n"\
					"############################################################\n"\
					"MlxBullseyeDump finished\n"\
					"############################################################\n"\
					);
#define MlxBullseyeInit()\
	printf ( "\n\n"\
				"############################################################\n"\
				"Bullseye initialized\n"\
				"############################################################\n"\
				);

#else

#define MlxBullseyeDump()
#define MlxBullseyeInit()

#endif


#endif /* SRC_BULLSEYE_MLX_BULLSEYE_H_ */
