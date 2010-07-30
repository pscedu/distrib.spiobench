###########################################################################
#
# Makefile - Makefile to build SPIObench
#
# This work contains valuable confidential and proprietary information.
# Disclosure, use or reproduction without the written authorization of
# the  DoD HPCMO is prohibited.  This unpublished work by the DoD HPCMO
# is protected by the laws of the United States and other countries.
#
# Description:
# Makefile for the SPIO benchmark.
#
# Remove comments of the compile flags for
# the appropriate benchmark platform. If 
# your platform is not listed, create a 
# section for your machine.
#
# The CC, CFLAGS and CLIBS variables are
# based on previous benchmark results from
# vendors and tests on DOD/MSRC machines.
# They may not be optimal, but they do
# provide a starting point.
#
# To build:
#	make
#
# To execute:
#	See the ./README file for instructions.
#
###########################################################################

######### Sun Compile Flags #########
#
# CC = cc
# CFLAGS = -fnonstd -fast -xO4 -native
# CLIBS = -lmpi -lm
#
#####################################

######## Compaq Compile Flags #######
#
# CC = cc
# CFLAGS = -fast -arch host -tune host
# CLIBS = -lmpi -lm -lelan
#
#####################################
 
######### Cray Compile Flags ########
#
# CC = cc
# CFLAGS = -h scalar3  -h aggress -h restrict=a
# CLIBS = -lmpi
#
#####################################
 
######### IBM Compile Flags #########
#
# CC = mpcc_r
# CFLAGS = -O4 -q64 -qarch=auto
# CLIBS = -lmpi
#
#####################################

##### Linux/Intel Compile Flags #####
#
# CC = mpicc
#CC=mpicc
#CFLAGS=-g -I${HOME}/install/include
#CLIBS=-L${HOME}/install/lib -lzintercept -lzestrpc -lzlnet -lsocknal  -lzcfs 
#
#####################################

########  SGI Compile Flags   #######
#
# CC = cc
# CFLAGS = -64 -Ofast
# CLIBS = -lmpi
#
#####################################

##### Cray XT3 (Linux) Flags #####
# --> plain (no ZEST)
#CC = mpicc
#CFLAGS=-g
#

# with ZEST
CC=		zcc
CFLAGS=		-Wall -W -g -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
		-I/opt/xt-mpt/default/mpich2-64/P2/include
#CFLAGS+=	-nolustre
CLIBS=		-L/opt/xt-mpt/default/mpich2-64/P2/lib -lmpich

# -lzlnet -lsocknal  -lzcfs 
#
#####################################

all: clean spio 

spio:
	$(CC) $(CFLAGS) -o spio src/spio.c ${CLIBS}

clean:
	rm -f spio 

tar:
	rm -f ../spiobench_results.tar
	cd .. ; tar cvf spiobench_results.tar spiobench

