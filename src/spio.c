/*
 * spio.c - Source file for SPIO Benchmark
 *
 * This work contains valuable confidential and proprietary information.
 * Disclosure, use or reproduction without the written authorization of
 * the  DoD HPCMO is prohibited.  This unpublished work by the DoD HPCMO
 * is protected by the laws of the United States and other countries.
 *
 * Author:      Nathan Schumann	
 *		2004/06/21	Original streamingio benchmark
 *
 *		Michael Timmerman
 *		2005/04/12	added MPI support 
 *			  	added node tracking
 *			  	added parameters to support configurations to mimic
 *				out of core codes
 *				renamed to SPIObench
 *
 * Description:
 * spio.c is the SPIO Benchmark source file. Using various arguments
 * (explained below) the program will perform writes and several different
 * types of reads.
 *
 * Compilation:
 * Use make to compile.
 *
 * The 64-bit_flags is to be substituted for whatever flags are supported
 * by your compiler for 64-bit compilation.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include "mpi.h"

#include <sys/types.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(SOLARIS)
typedef unsigned long long	unsigned long long;
#endif

#define MAX_PROCESSES	8
#define LL_SIZE		sizeof(long long)
#define KB		1024
#define MB		1024*1024
#define GB		1024*1024*1024
#define TB		1024*1024*1024*1024

#ifdef XT3
#define XALLOC malloc
#else
#define XALLOC valloc
#endif

struct timeval run_times[2];
#define STARTWATCH ( gettimeofday(&(run_times[0]), NULL) )
#define STOPWATCH  ( gettimeofday(&(run_times[1]), NULL) )

static const char * operations[] = {
	#define	O_WRITES	0
	"sw:	sequential write",
	#define O_WRITEB	1
	"bw:	backward write",
	#define O_READS		2
	"sr:	sequential read",
	#define O_READR		3
	"rr:	random read",
	#define O_READB		4
	"br:	backward read",
	#define O_READW		5
	"rw:	random read_write",
	#define O_WRITET	6
	"tw:	test write",
	NULL
};

static const char * iopts[] = {
	#define	O_WRITES	0
	"sw",
	#define O_WRITEB	1
	"bw",
	#define O_READS		2
	"sr",
	#define O_READR		3
	"rr",
	#define O_READB		4
	"br",
	#define O_READW		5
	"rw",
	#define O_WRITET	6
	"tw",
	NULL
};

extern int errno;
extern int optind, opterr, optopt;
extern char * optarg;

static void errno_abort(char * text, char * file, int line, int error_number);
static unsigned long long mknum(char * str);
static void usage();

static float calc_run_time() { 
  float t;

  t = ( ((run_times[1].tv_usec/1000000.0) + run_times[1].tv_sec) - 
        ((run_times[0].tv_usec/1000000.0) + run_times[0].tv_sec) );
  return t;
}


int main(int argc, char ** argv)
{
	char dfile[256];
	char basedfile[256];
	char ofile[256];
	char baseofile[256];
	char *htype, *ttype, *tname;
	long long pattern = 1010101010101010;
	long long *s_buff;
	ssize_t f_iter, b_iter, r, w;
	unsigned long long fsize = 0;
	unsigned long long bsize = 0;
	unsigned long long stride = 0;
	off_t count, last, range, offset;
	clock_t start, end, realtime, usertime, systime, clk_tck;
	struct tms t0, t1;
	int mode = -1;
	int fd = -1;
	int i;
	int c;
	int error = 0;
	FILE * fp;
	int do_output = 1;
	int ncpus = 1;
	int nprocs, my_proc;
	int test_seq_num = 0;

	if (argc < 2)
		usage();

	while((c = getopt(argc, argv, "hs:b:i:n:S:")) != EOF)
	{
		switch(c)
		{
			case 'h':
				usage();
				break;
			case 's':
				if (optarg)
					fsize = mknum(optarg);
				else
					error++;
				break;
			case 'b':
				if (optarg)
					bsize = mknum(optarg);
				else
					error++;
				break;
			case 'i':
				for (i = 0; i < 7; i++)
				{
					if (strcmp(iopts[i], optarg) == 0)
					{
						mode = i;
						break;
					}
				}
				break;
			case 'n':
				if (optarg)
					test_seq_num = mknum(optarg);
				else
					error++;
				break;
			case 'S':
				if (optarg)
					stride = mknum(optarg);
				else
					error++;
				break;
			case '?':
				usage();
		}
	}

	if (optind == argc)
		error++;

	if (error > 0)
		usage();

	(void)strncpy(basedfile, argv[optind++], 256);

	switch (mode)
	{
		case O_WRITES:
			ttype = "Sequential Write";
			tname = "spio01";
			break;
		case O_READS:
			ttype = "Sequential Read";
			tname = "spio02";
			break;
		case O_WRITEB:
			ttype = "Write Backwards";
			tname = "spio03";
			break;
		case O_READR:
			ttype = "Random Read";
			tname = "spio04";
			break;
		case O_READB:
			ttype = "Backward Read";
			tname = "spio05";
			break;
		case O_READW:
			ttype = "Random Read Rewrite";
			tname = "spio06";
			break;
	}

	htype = "FS I/O";

	srand48(5);

	clk_tck = sysconf(_SC_CLK_TCK);

	b_iter = bsize / LL_SIZE;

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_proc);

	sprintf(dfile, "%s.%04i.%04i", basedfile, nprocs, my_proc);
	sprintf(ofile, "%s.%s.%04i.%04i.out", baseofile, tname, nprocs, test_seq_num);

	if (mode == O_WRITES)
	{
		f_iter = fsize / bsize;

		s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		for (i = 0; i < b_iter; i++)
			s_buff[i] = pattern;
		MPI_Barrier(MPI_COMM_WORLD);

		STARTWATCH;

		if ((fd = open(dfile, O_WRONLY | O_CREAT, 0777)) < 0)
		{ 
			(void)fprintf(stderr, "Unable to open %s for writing <%s>\n", dfile, strerror(errno));
			exit(errno);
		}
		for (i = 0; i < f_iter; i++)
		{
			w = write(fd, s_buff, bsize); 
			if (w != bsize)
				errno_abort("Unable to write to data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);

		STOPWATCH;		

		free(s_buff);
	}

	if (mode == O_WRITEB)
	{
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		for (i = 0; i < b_iter; i++)
			s_buff[i] = pattern;
		MPI_Barrier(MPI_COMM_WORLD);
		
		STARTWATCH;

		if ((fd = open(dfile, O_WRONLY)) < 0)
			errno_abort("Unable to open file for writing", __FILE__, __LINE__, errno);
		if ((last = lseek(fd, 0L, SEEK_END)) < 0)
			errno_abort("Unable to seek in data file", __FILE__, __LINE__, errno);
		for (count = bsize; count <= last; count += bsize)
		{
			if (lseek(fd, -count, SEEK_END) < 0)
				errno_abort("Unable to seek in data file", __FILE__, __LINE__, errno);
			w = write(fd, s_buff, bsize);
			if ( w != bsize)
				errno_abort("Unable to write to data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);

		STOPWATCH;

		free(s_buff);
	}

	if (mode == O_WRITET)
	{
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		for (i = 0; i < b_iter; i++)
			s_buff[i] = pattern;
		MPI_Barrier(MPI_COMM_WORLD);

		STARTWATCH;

		if ((fd = open(dfile, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
			errno_abort("Unable to open file for writing", __FILE__, __LINE__, errno);
		for (i = 0; i < f_iter; i++)
		{
			w = write(fd, s_buff, bsize);
			if (w != bsize)
				errno_abort("Unable to write to data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);

		STOPWATCH;

		free(s_buff);
	}

	if (mode == O_READR)
	{
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		MPI_Barrier(MPI_COMM_WORLD);

		STARTWATCH;

		if ((fd = open(dfile, O_RDONLY)) < 0)
			errno_abort("Unable to open file for reading", __FILE__, __LINE__, errno);
		for (i = 0; i < f_iter; i++)
		{
			range = fsize - bsize;
			offset = (off_t)((double)range * drand48());
			offset = (offset / bsize) * bsize;
			if (lseek(fd, offset, SEEK_SET) < 0)
				errno_abort("Unable to seek in data file", __FILE__, __LINE__, errno);
			r = read(fd, s_buff, bsize);
			if ( r != bsize)
				errno_abort("Unable to read from data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);

		STOPWATCH;

		free(s_buff);
	}

	if (mode == O_READB)
	{
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		MPI_Barrier(MPI_COMM_WORLD);

		STARTWATCH;

		if ((fd = open(dfile, O_RDONLY)) < 0)
			errno_abort("Unable to open file for reading", __FILE__, __LINE__, errno);
		if ((last = lseek(fd, 0L, SEEK_END)) < 0)
			errno_abort("Unable to seek in data file", __FILE__, __LINE__, errno);
		for (count = bsize; count <= last; count += bsize)
		{
			if (lseek(fd, -count, SEEK_END) < 0)
				errno_abort("Unable to seek in data file", __FILE__, __LINE__, errno);
			r = read(fd, s_buff, bsize);
			if ( r != bsize)
				errno_abort("Unable to read from data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);

		STOPWATCH;

		free(s_buff);
	}

	if (mode == O_READS)
	{
		int do_stride = 0;
		unsigned long long offset;
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		MPI_Barrier(MPI_COMM_WORLD);

		STARTWATCH;

		if ((fd = open(dfile, O_RDONLY)) < 0)
			errno_abort("Unable to open file for reading", __FILE__, __LINE__, errno);
		for (i = 0 ; i < f_iter ; i++)
		{
			if (stride > 0 && do_stride == 1)
			{
				if ((offset = lseek(fd, 0, SEEK_CUR)) < 0)
					errno_abort("Unable to get current file position", __FILE__, __LINE__, errno);

				if (lseek(fd, (offset+stride), SEEK_SET) < 0)
					errno_abort("Unable to seek in file", __FILE__, __LINE__, errno);

				/* Very specific for BMI from 2002 will have to be changed for future revisions. */
				i += 12800;
				do_stride = 0;
			}
			r = read(fd, s_buff, bsize);
			if (r != bsize )
				errno_abort("Unable to read from data file", __FILE__, __LINE__, errno);

			if ((i % 6400) == 0 && i != 0)
				do_stride = 1;
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);
		STOPWATCH;
		free(s_buff);
	}

	if (mode == O_READW)
	{
		f_iter = fsize / bsize;
                s_buff = (long long *)XALLOC(b_iter * LL_SIZE);

		MPI_Barrier(MPI_COMM_WORLD);
		STARTWATCH;
		if ((fd = open(dfile, O_RDWR)) < 0)
		{
			(void)fprintf(stderr, "Unable to open %s for reading/writing <%s>\n", dfile, strerror(errno));
			exit(errno);
		}
		for (i = 0; i < f_iter; i++)
		{
			range = fsize - bsize;
			offset = (off_t)((double)range * drand48());
			offset = (offset / bsize) * bsize;
			if (lseek(fd, offset, SEEK_SET) < 0)
				errno_abort("Unable to seek in file", __FILE__, __LINE__, errno);
			r = read(fd, s_buff, bsize);
			if (r != bsize)
				errno_abort("Unable to read from data file", __FILE__, __LINE__, errno);
			if (lseek(fd, offset, SEEK_SET) < 0)
				errno_abort("Unable to seek in file", __FILE__, __LINE__, errno);
			w = write(fd, s_buff, bsize);
			if (w != bsize)
				errno_abort("Unable to write to data file", __FILE__, __LINE__, errno);
		}
		if (close(fd) < 0)
			errno_abort("Unable to close data file", __FILE__, __LINE__, errno);
		STOPWATCH;
		free(s_buff);
	}

	if (mode != O_WRITET)
	{
		float	*realtime_a, *usertime_a, *systime_a;
		float	realtime_l, usertime_l, systime_l;
		int	i;
		char nodesname[MPI_MAX_PROCESSOR_NAME], *nodenames;
		int nodesnamelength;
		struct	node_info	{
				int	length;
				char	name[MPI_MAX_PROCESSOR_NAME];
			} local_node, *global_nodes;
		
		realtime_l = calc_run_time();
		
		printf("%.5f --=--  \n", realtime_l);

		MPI_Get_processor_name( local_node.name, &local_node.length);
		global_nodes = (struct node_info *)malloc(nprocs*sizeof(struct node_info));
		MPI_Gather(	(void *)&local_node, sizeof(struct node_info), MPI_BYTE,
				(void *)global_nodes, sizeof(struct node_info), MPI_BYTE,
				0, MPI_COMM_WORLD );

		realtime_a = (float *)malloc(nprocs*sizeof(float));
		MPI_Gather(	(void *)&realtime_l, sizeof(float), MPI_BYTE,
				(void *)realtime_a , sizeof(float), MPI_BYTE,
				0, MPI_COMM_WORLD );

		if( my_proc == 0 ) {
			printf("### Start Perl Output ###\n");
			printf("$tstDescript{\"sTestNAME\"}    = \"%s\";\n", tname);
			printf("$tstDescript{\"sFileNAME\"}    = \"%s\";\n", "spiobench.c");
			printf("$tstDescript{\"NCPUS\"}        = %i;\n", nprocs);
			printf("$tstDescript{\"CLKTICK\"}      = %i;\n", clk_tck);
			printf("$tstDescript{\"TestDescript\"} = \"%s\";\n", ttype);
			printf("$tstDescript{\"PRECISION\"}    = \"%s\";\n", "N/A");
			printf("$tstDescript{\"LANG\"}         = \"%s\";\n", "C");
			printf("$tstDescript{\"VERSION\"}      = \"%s\";\n", "6.0");
			printf("$tstDescript{\"PERL_BLOCK\"}   = \"%s\";\n", "6.0");
			printf("$tstDescript{\"TI_Release\"}   = \"%s\";\n", "TI-06");
			printf("$tstDescData[0] = \"Test Sequence Number\";\n");
			printf("$tstDescData[1] = \"File Size [Bytes]\";\n");
			printf("$tstDescData[2] = \"Transfer Size [Bytes]\";\n");
			printf("$tstDescData[3] = \"Number of Transfers\";\n");
			printf("$tstDescData[4] = \"Real Time [secs]\";\n");

			for( i = 0 ; i < nprocs ; i++ ) {
				printf("$tstData[%4i][0] = %i;\n",   i, test_seq_num);
				printf("$tstData[%4i][1] = %lld;\n", i, (long long)fsize);
				printf("$tstData[%4i][2] = %lld;\n", i, (long long)bsize);
				printf("$tstData[%4i][3] = %d;\n",   i, f_iter);
				printf("$tstData[%4i][4] = %.5f;\n", i, realtime_a[i]);
			}
			printf("$tstDescDataT[0] = \"Node Name\";\n");
			for( i = 0 ; i < nprocs ; i++ ) {
				printf("$tstDataT[%4i][0] = \"%s\";\n",   i, global_nodes[i].name);
			}
			printf("### End Perl Output ###\n");
		}
	}

	MPI_Barrier( MPI_COMM_WORLD );
	MPI_Finalize();
	return(0);
}


static unsigned long long mknum(char * str)
{
	unsigned long long val;
	char * suffix;

	val = strtoul(str, &suffix, 0);

	while(*suffix == ' ')
	{
		suffix++;
	}

	if (*suffix == 'k' || *suffix == 'K') {
		val *= KB;
	} else if (*suffix == 'm' || *suffix == 'M') {
		val *= MB;
	} else if (*suffix == 'g' || *suffix == 'G') {
		val *= GB;
	} else if (*suffix == 't' || *suffix == 'T') {
		val *= TB;
		val *= 1024;
	}

	return(val);
}

static void errno_abort(char * text, char * file, int line, int error_number)
{
	(void)fprintf(stderr, "%s at \"%s\":%d: %s\n", text, file, line, strerror(error_number));
	exit(errno);
}

static void usage()
{
	int i = 0;
	(void)printf("Usage: iobench [ -h ] [ -n test_seq_number ] [ -r report_file ] -s file_size(k,m,g) -b block_size(k,m,g) -i operation [ -S stride(k,m,g) ] data_file\n\n");
	(void)printf("Operations are:\n");
	for (i = 0; operations[i] != NULL; i++)
		(void)printf("\t%s\n", operations[i]);
	exit(0);
}

