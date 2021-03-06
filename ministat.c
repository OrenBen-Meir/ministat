/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */
#include <sys/ioctl.h>

#include <err.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h> //NEW HEADER
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdbool.h>

#include "strtod.h"
#include "queue.h"

#define FILECHUNK_BUFFSIZE 50*BUFSIZ // size of buffer used for large chunks of text files

#define NSTUDENT 100
#define NCONF 6

// setup an_qsort doubles
static int
dbl_cmp(const void *a, const void *b);

#define AN_QSORT_SUFFIX doubles
#define AN_QSORT_TYPE double
#define AN_QSORT_CMP dbl_cmp

#include "an_qsort.inc"
// setup an_qsort int
static int
int_cmp(const void *a, const void *b);

#define AN_QSORT_SUFFIX ints
#define AN_QSORT_TYPE int
#define AN_QSORT_CMP int_cmp

#include "an_qsort.inc"
// end setup an_qsort

int flag_vt = 0; //Verbose timing flag (set to be global)

#define ITERATIONS 1 //This might need to change.
static unsigned long long int timeStrtok = 0;
static unsigned long long int timeStrtod = 0;
static unsigned long long int timeSort = 0;

struct timespec tstart, tstop;

void gettime_ifflagged(struct timespec *tp) {
	if(__builtin_expect(flag_vt == 1, false)) // this ensures for branch prediction that this condition at most times fails
		clock_gettime(CLOCK_MONOTONIC, tp);
}

static unsigned long long elapsed_us(struct timespec *a, struct timespec *b)
{
	unsigned long long a_p = (a->tv_sec * 1000000ULL) + a->tv_nsec/1000;
	unsigned long long b_p = (b->tv_sec * 1000000ULL) + b->tv_nsec/1000;
	
	return b_p - a_p;
}

static void add_elapsed_time(unsigned long long *dest, struct timespec *a, struct timespec *b) {
	if(__builtin_expect(flag_vt == 1, false)) {
		*dest += elapsed_us(a, b) / ITERATIONS;
	}
}

double const studentpct[] = { 80, 90, 95, 98, 99, 99.5 };
double student [NSTUDENT + 1][NCONF] = {
/* inf */	{	1.282,	1.645,	1.960,	2.326,	2.576,	3.090  },
/* 1. */	{	3.078,	6.314,	12.706,	31.821,	63.657,	318.313  },
/* 2. */	{	1.886,	2.920,	4.303,	6.965,	9.925,	22.327  },
/* 3. */	{	1.638,	2.353,	3.182,	4.541,	5.841,	10.215  },
/* 4. */	{	1.533,	2.132,	2.776,	3.747,	4.604,	7.173  },
/* 5. */	{	1.476,	2.015,	2.571,	3.365,	4.032,	5.893  },
/* 6. */	{	1.440,	1.943,	2.447,	3.143,	3.707,	5.208  },
/* 7. */	{	1.415,	1.895,	2.365,	2.998,	3.499,	4.782  },
/* 8. */	{	1.397,	1.860,	2.306,	2.896,	3.355,	4.499  },
/* 9. */	{	1.383,	1.833,	2.262,	2.821,	3.250,	4.296  },
/* 10. */	{	1.372,	1.812,	2.228,	2.764,	3.169,	4.143  },
/* 11. */	{	1.363,	1.796,	2.201,	2.718,	3.106,	4.024  },
/* 12. */	{	1.356,	1.782,	2.179,	2.681,	3.055,	3.929  },
/* 13. */	{	1.350,	1.771,	2.160,	2.650,	3.012,	3.852  },
/* 14. */	{	1.345,	1.761,	2.145,	2.624,	2.977,	3.787  },
/* 15. */	{	1.341,	1.753,	2.131,	2.602,	2.947,	3.733  },
/* 16. */	{	1.337,	1.746,	2.120,	2.583,	2.921,	3.686  },
/* 17. */	{	1.333,	1.740,	2.110,	2.567,	2.898,	3.646  },
/* 18. */	{	1.330,	1.734,	2.101,	2.552,	2.878,	3.610  },
/* 19. */	{	1.328,	1.729,	2.093,	2.539,	2.861,	3.579  },
/* 20. */	{	1.325,	1.725,	2.086,	2.528,	2.845,	3.552  },
/* 21. */	{	1.323,	1.721,	2.080,	2.518,	2.831,	3.527  },
/* 22. */	{	1.321,	1.717,	2.074,	2.508,	2.819,	3.505  },
/* 23. */	{	1.319,	1.714,	2.069,	2.500,	2.807,	3.485  },
/* 24. */	{	1.318,	1.711,	2.064,	2.492,	2.797,	3.467  },
/* 25. */	{	1.316,	1.708,	2.060,	2.485,	2.787,	3.450  },
/* 26. */	{	1.315,	1.706,	2.056,	2.479,	2.779,	3.435  },
/* 27. */	{	1.314,	1.703,	2.052,	2.473,	2.771,	3.421  },
/* 28. */	{	1.313,	1.701,	2.048,	2.467,	2.763,	3.408  },
/* 29. */	{	1.311,	1.699,	2.045,	2.462,	2.756,	3.396  },
/* 30. */	{	1.310,	1.697,	2.042,	2.457,	2.750,	3.385  },
/* 31. */	{	1.309,	1.696,	2.040,	2.453,	2.744,	3.375  },
/* 32. */	{	1.309,	1.694,	2.037,	2.449,	2.738,	3.365  },
/* 33. */	{	1.308,	1.692,	2.035,	2.445,	2.733,	3.356  },
/* 34. */	{	1.307,	1.691,	2.032,	2.441,	2.728,	3.348  },
/* 35. */	{	1.306,	1.690,	2.030,	2.438,	2.724,	3.340  },
/* 36. */	{	1.306,	1.688,	2.028,	2.434,	2.719,	3.333  },
/* 37. */	{	1.305,	1.687,	2.026,	2.431,	2.715,	3.326  },
/* 38. */	{	1.304,	1.686,	2.024,	2.429,	2.712,	3.319  },
/* 39. */	{	1.304,	1.685,	2.023,	2.426,	2.708,	3.313  },
/* 40. */	{	1.303,	1.684,	2.021,	2.423,	2.704,	3.307  },
/* 41. */	{	1.303,	1.683,	2.020,	2.421,	2.701,	3.301  },
/* 42. */	{	1.302,	1.682,	2.018,	2.418,	2.698,	3.296  },
/* 43. */	{	1.302,	1.681,	2.017,	2.416,	2.695,	3.291  },
/* 44. */	{	1.301,	1.680,	2.015,	2.414,	2.692,	3.286  },
/* 45. */	{	1.301,	1.679,	2.014,	2.412,	2.690,	3.281  },
/* 46. */	{	1.300,	1.679,	2.013,	2.410,	2.687,	3.277  },
/* 47. */	{	1.300,	1.678,	2.012,	2.408,	2.685,	3.273  },
/* 48. */	{	1.299,	1.677,	2.011,	2.407,	2.682,	3.269  },
/* 49. */	{	1.299,	1.677,	2.010,	2.405,	2.680,	3.265  },
/* 50. */	{	1.299,	1.676,	2.009,	2.403,	2.678,	3.261  },
/* 51. */	{	1.298,	1.675,	2.008,	2.402,	2.676,	3.258  },
/* 52. */	{	1.298,	1.675,	2.007,	2.400,	2.674,	3.255  },
/* 53. */	{	1.298,	1.674,	2.006,	2.399,	2.672,	3.251  },
/* 54. */	{	1.297,	1.674,	2.005,	2.397,	2.670,	3.248  },
/* 55. */	{	1.297,	1.673,	2.004,	2.396,	2.668,	3.245  },
/* 56. */	{	1.297,	1.673,	2.003,	2.395,	2.667,	3.242  },
/* 57. */	{	1.297,	1.672,	2.002,	2.394,	2.665,	3.239  },
/* 58. */	{	1.296,	1.672,	2.002,	2.392,	2.663,	3.237  },
/* 59. */	{	1.296,	1.671,	2.001,	2.391,	2.662,	3.234  },
/* 60. */	{	1.296,	1.671,	2.000,	2.390,	2.660,	3.232  },
/* 61. */	{	1.296,	1.670,	2.000,	2.389,	2.659,	3.229  },
/* 62. */	{	1.295,	1.670,	1.999,	2.388,	2.657,	3.227  },
/* 63. */	{	1.295,	1.669,	1.998,	2.387,	2.656,	3.225  },
/* 64. */	{	1.295,	1.669,	1.998,	2.386,	2.655,	3.223  },
/* 65. */	{	1.295,	1.669,	1.997,	2.385,	2.654,	3.220  },
/* 66. */	{	1.295,	1.668,	1.997,	2.384,	2.652,	3.218  },
/* 67. */	{	1.294,	1.668,	1.996,	2.383,	2.651,	3.216  },
/* 68. */	{	1.294,	1.668,	1.995,	2.382,	2.650,	3.214  },
/* 69. */	{	1.294,	1.667,	1.995,	2.382,	2.649,	3.213  },
/* 70. */	{	1.294,	1.667,	1.994,	2.381,	2.648,	3.211  },
/* 71. */	{	1.294,	1.667,	1.994,	2.380,	2.647,	3.209  },
/* 72. */	{	1.293,	1.666,	1.993,	2.379,	2.646,	3.207  },
/* 73. */	{	1.293,	1.666,	1.993,	2.379,	2.645,	3.206  },
/* 74. */	{	1.293,	1.666,	1.993,	2.378,	2.644,	3.204  },
/* 75. */	{	1.293,	1.665,	1.992,	2.377,	2.643,	3.202  },
/* 76. */	{	1.293,	1.665,	1.992,	2.376,	2.642,	3.201  },
/* 77. */	{	1.293,	1.665,	1.991,	2.376,	2.641,	3.199  },
/* 78. */	{	1.292,	1.665,	1.991,	2.375,	2.640,	3.198  },
/* 79. */	{	1.292,	1.664,	1.990,	2.374,	2.640,	3.197  },
/* 80. */	{	1.292,	1.664,	1.990,	2.374,	2.639,	3.195  },
/* 81. */	{	1.292,	1.664,	1.990,	2.373,	2.638,	3.194  },
/* 82. */	{	1.292,	1.664,	1.989,	2.373,	2.637,	3.193  },
/* 83. */	{	1.292,	1.663,	1.989,	2.372,	2.636,	3.191  },
/* 84. */	{	1.292,	1.663,	1.989,	2.372,	2.636,	3.190  },
/* 85. */	{	1.292,	1.663,	1.988,	2.371,	2.635,	3.189  },
/* 86. */	{	1.291,	1.663,	1.988,	2.370,	2.634,	3.188  },
/* 87. */	{	1.291,	1.663,	1.988,	2.370,	2.634,	3.187  },
/* 88. */	{	1.291,	1.662,	1.987,	2.369,	2.633,	3.185  },
/* 89. */	{	1.291,	1.662,	1.987,	2.369,	2.632,	3.184  },
/* 90. */	{	1.291,	1.662,	1.987,	2.368,	2.632,	3.183  },
/* 91. */	{	1.291,	1.662,	1.986,	2.368,	2.631,	3.182  },
/* 92. */	{	1.291,	1.662,	1.986,	2.368,	2.630,	3.181  },
/* 93. */	{	1.291,	1.661,	1.986,	2.367,	2.630,	3.180  },
/* 94. */	{	1.291,	1.661,	1.986,	2.367,	2.629,	3.179  },
/* 95. */	{	1.291,	1.661,	1.985,	2.366,	2.629,	3.178  },
/* 96. */	{	1.290,	1.661,	1.985,	2.366,	2.628,	3.177  },
/* 97. */	{	1.290,	1.661,	1.985,	2.365,	2.627,	3.176  },
/* 98. */	{	1.290,	1.661,	1.984,	2.365,	2.627,	3.175  },
/* 99. */	{	1.290,	1.660,	1.984,	2.365,	2.626,	3.175  },
/* 100. */	{	1.290,	1.660,	1.984,	2.364,	2.626,	3.174  }
};

#define	MAX_DS	8
static char symbol[MAX_DS] = { ' ', 'x', '+', '*', '%', '#', '@', 'O' };

struct dataset {
	char *name;
	
	struct linkedListNode* head;
	struct linkedListNode* tail;
	
	double *points;
	double sy, syy;
	unsigned n;
};

struct dataset_int {
	char *name;
	
	struct linkedListNode_int* head;
	struct linkedListNode_int* tail;
	
	int *points;
	double sy, syy;
	unsigned n;
};

#define LPOINTS 100000 // node points capacity

struct linkedListNode {
	double points[LPOINTS];
	unsigned n;
	struct linkedListNode* next;
};

struct linkedListNode_int {
	int points[LPOINTS];
	unsigned n;
	struct linkedListNode_int* next;
};

static double * concatenateList(struct dataset *ds) //Turn the linked list into one big array to store in points.
{
	double * points = malloc(ds->n * sizeof(*ds->points));
	unsigned int points_i = 0;
	struct linkedListNode * cursor;
	for(cursor = ds->head; cursor != NULL; cursor = cursor->next)
	{	
		memcpy(points + points_i, cursor->points, cursor->n*sizeof(*cursor->points));
		points_i += cursor->n;
	}
	
	return points;
}

static int * concatenateList_int(struct dataset_int *ds) //Turn the linked list into one big array to store in points.
{
	int * points = malloc(ds->n * sizeof(*ds->points));
	unsigned int points_i = 0;
	struct linkedListNode_int * cursor;
	for(cursor = ds->head; cursor != NULL; cursor = cursor->next)
	{	
		memcpy(points + points_i, cursor->points, cursor->n*sizeof(*cursor->points));
		points_i += cursor->n;
	}
	
	return points;
}

static struct dataset *
NewSet(void)
{
	struct dataset *ds;
	struct linkedListNode *head;
	
	ds = calloc(1, sizeof *ds);
	ds->head = calloc(1, sizeof *head);
	// ds->lpoints = 100000;
	// ds->head->points = calloc(ds->lpoints, sizeof *ds->head->points);
	ds->head->next = NULL;
	ds->tail = ds->head;
	return(ds);
}

static struct dataset_int *
NewSet_int(void)
{
	struct dataset_int *ds;
	struct linkedListNode_int *head;
	
	ds = calloc(1, sizeof *ds);
	ds->head = calloc(1, sizeof *head);
	// ds->lpoints = 100000;
	// ds->head->points = calloc(ds->lpoints, sizeof *ds->head->points);
	ds->head->next = NULL;
	ds->tail = ds->head;
	return(ds);
}

static void
AddPoint(struct dataset *ds, double a)
{
	// double *dp;

	if (ds->tail->n >= LPOINTS) {
		//dp = ds->points;
		//ds->lpoints *= 4; !!!~~~		
		//ds->points = realloc(ds->points, sizeof *dp * ds->n); !!!~~~
		struct linkedListNode* newTail = (struct linkedListNode*) malloc(sizeof(struct linkedListNode));
		// newTail->points = calloc(ds->lpoints, sizeof *newTail->points);
		newTail->n = 0; // ensure n is by default 0
		ds->tail->next = newTail;
		ds->tail = newTail;
	}
	ds->tail->points[ds->tail->n++] = a;
	ds->n++;
	ds->sy += a;
	ds->syy += a * a;
	//ds->points = concatenateList(ds);
}

static void
AddPoint_int(struct dataset_int *ds, int a)
{
	// double *dp;

	if (ds->tail->n >= LPOINTS) {
		//dp = ds->points;
		//ds->lpoints *= 4; !!!~~~		
		//ds->points = realloc(ds->points, sizeof *dp * ds->n); !!!~~~
		struct linkedListNode_int* newTail = (struct linkedListNode_int*) malloc(sizeof(struct linkedListNode_int));
		// newTail->points = calloc(ds->lpoints, sizeof *newTail->points);
		newTail->n = 0; // ensure n is by default 0
		ds->tail->next = newTail;
		ds->tail = newTail;
	}
	ds->tail->points[ds->tail->n++] = a;
	ds->n++;
	ds->sy += (double)a;
	ds->syy += (double)a * (double)a;
	//ds->points = concatenateList(ds);
}

// merge contents of dateset other into the main dataset
static void merge_dataset(struct dataset* main, struct dataset* other) {
	main->tail->next = other->head;
	main->tail = other->tail;
	main->n += other->n;
	main->sy += other->sy;
	main->syy += other->syy;
}

static void merge_dataset_int(struct dataset_int* main, struct dataset_int* other) {
	main->tail->next = other->head;
	main->tail = other->tail;
	main->n += other->n;
	main->sy += other->sy;
	main->syy += other->syy;
}

static double
Min(struct dataset *ds)
{

	return (ds->points[0]);
}

static int
Min_int(struct dataset_int *ds)
{

	return (ds->points[0]);
}

static double
Max(struct dataset *ds)
{

	return (ds->points[ds->n -1]);
}

static int
Max_int(struct dataset_int *ds)
{

	return (ds->points[ds->n -1]);
}

static double
Avg(struct dataset *ds)
{

	return(ds->sy / ds->n);
}

static double
Avg_int(struct dataset_int *ds)
{

	return(ds->sy / ds->n);
}

static double
Median(struct dataset *ds)
{

	return (ds->points[ds->n / 2]);
}

static int
Median_int(struct dataset_int *ds)
{

	return (ds->points[ds->n / 2]);
}

static double
Var(struct dataset *ds)
{

	return (ds->syy - ds->sy * ds->sy / ds->n) / (ds->n - 1.0);
}

static double
Var_int(struct dataset_int *ds)
{

	return (ds->syy - ds->sy * ds->sy / ds->n) / (ds->n - 1.0);
}

static double
Stddev(struct dataset *ds)
{

	return sqrt(Var(ds));
}

static double
Stddev_int(struct dataset_int *ds)
{

	return sqrt(Var_int(ds));
}

static void
VitalsHead(void)
{

	printf("    N           Min           Max        Median           Avg        Stddev\n");
}

static void
Vitals(struct dataset *ds, int flag)
{

	printf("%c %3d %13.8g %13.8g %13.8g %13.8g %13.8g", symbol[flag],
	    ds->n, Min(ds), Max(ds), Median(ds), Avg(ds), Stddev(ds));
	printf("\n");
}

static void
Vitals_int(struct dataset_int *ds, int flag)
{

	printf("%c %3d %13.8d %13.8d %13.8d %13.8g %13.8g", symbol[flag],
	    ds->n, Min_int(ds), Max_int(ds), Median_int(ds), Avg_int(ds), Stddev_int(ds));
	printf("\n");
}

static void
Relative(struct dataset *ds, struct dataset *rs, int confidx)
{
	double spool, s, d, e, t;
	int i;

	i = ds->n + rs->n - 2;
	if (i > NSTUDENT)
		t = student[0][confidx];
	else
		t = student[i][confidx];
	spool = (ds->n - 1) * Var(ds) + (rs->n - 1) * Var(rs);
	spool /= ds->n + rs->n - 2;
	spool = sqrt(spool);
	s = spool * sqrt(1.0 / ds->n + 1.0 / rs->n);
	d = Avg(ds) - Avg(rs);
	e = t * s;

	if (fabs(d) > e) {
	
		printf("Difference at %.1f%% confidence\n", studentpct[confidx]);
		printf("	%g +/- %g\n", d, e);
		printf("	%g%% +/- %g%%\n", d * 100 / Avg(rs), e * 100 / Avg(rs));
		printf("	(Student's t, pooled s = %g)\n", spool);
	} else {
		printf("No difference proven at %.1f%% confidence\n",
		    studentpct[confidx]);
	}
}

static void
Relative_int(struct dataset_int *ds, struct dataset_int *rs, int confidx)
{
	double spool, s, d, e, t;
	int i;

	i = ds->n + rs->n - 2;
	if (i > NSTUDENT)
		t = student[0][confidx];
	else
		t = student[i][confidx];
	spool = (ds->n - 1) * Var_int(ds) + (rs->n - 1) * Var_int(rs);
	spool /= ds->n + rs->n - 2;
	spool = sqrt(spool);
	s = spool * sqrt(1.0 / ds->n + 1.0 / rs->n);
	d = Avg_int(ds) - Avg_int(rs);
	e = t * s;

	if (fabs(d) > e) {
	
		printf("Difference at %.1f%% confidence\n", studentpct[confidx]);
		printf("	%g +/- %g\n", d, e);
		printf("	%g%% +/- %g%%\n", d * 100 / Avg_int(rs), e * 100 / Avg_int(rs));
		printf("	(Student's t, pooled s = %g)\n", spool);
	} else {
		printf("No difference proven at %.1f%% confidence\n",
		    studentpct[confidx]);
	}
}

struct plot {
	double		min;
	double		max;
	double		span;
	int		width;

	double		x0, dx;
	int		height;
	char		*data;
	char		**bar;
	int		separate_bars;
	int		num_datasets;
};

static struct plot plot;

static void
SetupPlot(int width, int separate, int num_datasets)
{
	struct plot *pl;

	pl = &plot;
	pl->width = width;
	pl->height = 0;
	pl->data = NULL;
	pl->bar = NULL;
	pl->separate_bars = separate;
	pl->num_datasets = num_datasets;
	pl->min = 999e99;
	pl->max = -999e99;
}

static void
AdjPlot(double a)
{
	struct plot *pl;

	pl = &plot;
	if (a < pl->min)
		pl->min = a;
	if (a > pl->max)
		pl->max = a;
	pl->span = pl->max - pl->min;
	pl->dx = pl->span / (pl->width - 1.0);
	pl->x0 = pl->min - .5 * pl->dx;
}

static void
AdjPlot_int(int a)
{
	struct plot *pl;

	pl = &plot;
	if (a < pl->min)
		pl->min = a;
	if (a > pl->max)
		pl->max = a;
	pl->span = pl->max - pl->min;
	pl->dx = pl->span / (pl->width - 1.0);
	pl->x0 = pl->min - .5 * pl->dx;
}

static void
DimPlot(struct dataset *ds)
{
	AdjPlot(Min(ds));
	AdjPlot(Max(ds));
	AdjPlot(Avg(ds) - Stddev(ds));
	AdjPlot(Avg(ds) + Stddev(ds));
}

static void
DimPlot_int(struct dataset_int *ds)
{
	AdjPlot_int(Min_int(ds));
	AdjPlot_int(Max_int(ds));
	AdjPlot_int(Avg_int(ds) - Stddev_int(ds));
	AdjPlot_int(Avg_int(ds) + Stddev_int(ds));
}

static void
PlotSet(struct dataset *ds, int val)
{
	struct plot *pl;
	int i, j, m, x;
	unsigned n;
	int bar;

	pl = &plot;
	if (pl->span == 0)
		return;

	if (pl->separate_bars)
		bar = val-1;
	else
		bar = 0;

	if (pl->bar == NULL) {
		pl->bar = malloc(sizeof(char *) * pl->num_datasets);
		memset(pl->bar, 0, sizeof(char*) * pl->num_datasets);
	}
	if (pl->bar[bar] == NULL) {
		pl->bar[bar] = malloc(pl->width);
		memset(pl->bar[bar], 0, pl->width);
	}
	
	m = 1;
	i = -1;
	j = 0;
	for (n = 0; n < ds->n; n++) {
		x = (ds->points[n] - pl->x0) / pl->dx;
		if (x == i) {
			j++;
			if (j > m)
				m = j;
		} else {
			j = 1;
			i = x;
		}
	}
	m += 1;
	if (m > pl->height) {
		pl->data = realloc(pl->data, pl->width * m);
		memset(pl->data + pl->height * pl->width, 0,
		    (m - pl->height) * pl->width);
	}
	pl->height = m;
	i = -1;
	for (n = 0; n < ds->n; n++) {
		x = (ds->points[n] - pl->x0) / pl->dx;
		if (x == i) {
			j++;
		} else {
			j = 1;
			i = x;
		}
		pl->data[j * pl->width + x] |= val;
	}
	if (!isnan(Stddev(ds))) {
		x = ((Avg(ds) - Stddev(ds)) - pl->x0) / pl->dx;
		m = ((Avg(ds) + Stddev(ds)) - pl->x0) / pl->dx;
		pl->bar[bar][m] = '|';
		pl->bar[bar][x] = '|';
		for (i = x + 1; i < m; i++)
			if (pl->bar[bar][i] == 0)
				pl->bar[bar][i] = '_';
	}
	x = (Median(ds) - pl->x0) / pl->dx;
	pl->bar[bar][x] = 'M';
	x = (Avg(ds) - pl->x0) / pl->dx;
	pl->bar[bar][x] = 'A';
}

static void
PlotSet_int(struct dataset_int *ds, int val)
{
	struct plot *pl;
	int i, j, m, x;
	unsigned n;
	int bar;

	pl = &plot;
	if (pl->span == 0)
		return;

	if (pl->separate_bars)
		bar = val-1;
	else
		bar = 0;

	if (pl->bar == NULL) {
		pl->bar = malloc(sizeof(char *) * pl->num_datasets);
		memset(pl->bar, 0, sizeof(char*) * pl->num_datasets);
	}
	if (pl->bar[bar] == NULL) {
		pl->bar[bar] = malloc(pl->width);
		memset(pl->bar[bar], 0, pl->width);
	}
	
	m = 1;
	i = -1;
	j = 0;
	for (n = 0; n < ds->n; n++) {
		x = (ds->points[n] - pl->x0) / pl->dx;
		if (x == i) {
			j++;
			if (j > m)
				m = j;
		} else {
			j = 1;
			i = x;
		}
	}
	m += 1;
	if (m > pl->height) {
		pl->data = realloc(pl->data, pl->width * m);
		memset(pl->data + pl->height * pl->width, 0,
		    (m - pl->height) * pl->width);
	}
	pl->height = m;
	i = -1;
	for (n = 0; n < ds->n; n++) {
		x = (ds->points[n] - pl->x0) / pl->dx;
		if (x == i) {
			j++;
		} else {
			j = 1;
			i = x;
		}
		pl->data[j * pl->width + x] |= val;
	}
	if (!isnan(Stddev_int(ds))) {
		x = ((Avg_int(ds) - Stddev_int(ds)) - pl->x0) / pl->dx;
		m = ((Avg_int(ds) + Stddev_int(ds)) - pl->x0) / pl->dx;
		pl->bar[bar][m] = '|';
		pl->bar[bar][x] = '|';
		for (i = x + 1; i < m; i++)
			if (pl->bar[bar][i] == 0)
				pl->bar[bar][i] = '_';
	}
	x = (Median_int(ds) - pl->x0) / pl->dx;
	pl->bar[bar][x] = 'M';
	x = (Avg_int(ds) - pl->x0) / pl->dx;
	pl->bar[bar][x] = 'A';
}

static void
DumpPlot(void)
{
	struct plot *pl;
	int i, j, k;

	pl = &plot;
	if (pl->span == 0) {
		printf("[no plot, span is zero width]\n");
		return;
	}

	putchar('+');
	for (i = 0; i < pl->width; i++)
		putchar('-');
	putchar('+');
	putchar('\n');
	for (i = 1; i < pl->height; i++) {
		putchar('|');
		for (j = 0; j < pl->width; j++) {
			k = pl->data[(pl->height - i) * pl->width + j];
			if (k >= 0 && k < MAX_DS)
				putchar(symbol[k]);
			else
				printf("[%02x]", k);
		}
		putchar('|');
		putchar('\n');
	}
	for (i = 0; i < pl->num_datasets; i++) {
		if (pl->bar[i] == NULL)
			continue;
		putchar('|');
		for (j = 0; j < pl->width; j++) {
			k = pl->bar[i][j];
			if (k == 0)
				k = ' ';
			putchar(k);
		}
		putchar('|');
		putchar('\n');
	}
	putchar('+');
	for (i = 0; i < pl->width; i++)
		putchar('-');
	putchar('+');
	putchar('\n');
}

static int
dbl_cmp(const void *a, const void *b)
{
	const double *aa = a;
	const double *bb = b;

	if (*aa < *bb)
		return (-1);
	else if (*aa > *bb)
		return (1);
	else
		return (0);
}

static int
int_cmp(const void *a, const void *b)
{
	const int *aa = a;
	const int *bb = b;

	if (*aa < *bb)
		return (-1);
	else if (*aa > *bb)
		return (1);
	else
		return (0);
}

struct filechunkread_threadcontext {
	int fd;
	off_t filesize;
	off_t offset;
	int column;
	const char* row_delim;
	u_int64_t line_number;
	bool line_error_flag;
	const char* filename;
	struct dataset* output_dataset;
	unsigned long long timestrtod;
	unsigned long long timestrtok; 
};

struct filechunkread_threadcontext_int {
	int fd;
	off_t filesize;
	off_t offset;
	int column;
	const char* row_delim;
	u_int64_t line_number;
	bool line_error_flag;
	const char* filename;
	struct dataset_int* output_dataset;
	unsigned long long timestrtod;
	unsigned long long timestrtok; 
};

static void 
read_fileline_to_dataset(struct filechunkread_threadcontext* context, char* line) {

	struct timespec ttstart, ttstop;
	char *t, *p;
	double d;
	size_t i = strlen(line);
	context->line_number++;
	
	gettime_ifflagged(&ttstart); // start time
	char* nextStr = line;
	for (i = 1, t = strsep(&nextStr, context->row_delim);
			t != NULL && *t != '#';
			i++, t = strsep(&nextStr, context->row_delim)) {
		if (i == context->column)
			break;
	}
	gettime_ifflagged(&ttstop);
	add_elapsed_time(&context->timestrtok, &ttstart, &ttstop);  //Store amount of time spent on strtod in seconds
	
	if (t == NULL || *t == '#') {
		return;
	}
	
	gettime_ifflagged(&ttstart); // start time
	d = vim_strtod(t, &p);
	gettime_ifflagged(&ttstop);
	add_elapsed_time(&context->timestrtod, &ttstart, &ttstop);  //Store amount of time spent on strtod in seconds
	
	if (p != NULL && *p != '\0') {
		context->line_error_flag = true;
		return;
		// err(2, "Invalid data in %s\n", context->filename);
	}
	if (*line != '\0')
		AddPoint(context->output_dataset, d);
	return;
}

static void 
read_fileline_to_dataset_int(struct filechunkread_threadcontext_int* context, char* line) {

	struct timespec ttstart, ttstop;
	char *t, *p;
	int d;
	size_t i = strlen(line);
	context->line_number++;
	
	gettime_ifflagged(&ttstart); // start time
	char* nextStr = line;
	for (i = 1, t = strsep(&nextStr, context->row_delim);
			t != NULL && *t != '#';
			i++, t = strsep(&nextStr, context->row_delim)) {
		if (i == context->column)
			break;
	}
	gettime_ifflagged(&ttstop);
	add_elapsed_time(&context->timestrtok, &ttstart, &ttstop);  //Store amount of time spent on strtod in seconds
	
	if (t == NULL || *t == '#') {
		return;
	}
	
	gettime_ifflagged(&ttstart); // start time
	d = strtol(t, &p, 10); //STRING TO INTEGER
	gettime_ifflagged(&ttstop);
	add_elapsed_time(&context->timestrtod, &ttstart, &ttstop);  //Store amount of time spent on strtod in seconds
	
	if (p != NULL && *p != '\0') {
		context->line_error_flag = true;
		return;
		// err(2, "Invalid data in %s\n", context->filename);
	}
	if (*line != '\0')
		AddPoint_int(context->output_dataset, d);
	return;
}

static void 
fill_filechunk_data(struct filechunkread_threadcontext* context) {
	char *next_token, *current_token, *nextStr;
	off_t bytes_left, bytes_to_read;
	char buffer[FILECHUNK_BUFFSIZE + 1];
	ssize_t r;
	context->output_dataset = NewSet();
	context->line_number = 0;

	bytes_left = context->filesize - context->offset;
	bytes_to_read = (FILECHUNK_BUFFSIZE<bytes_left)?FILECHUNK_BUFFSIZE:bytes_left;
	r = pread(context->fd, (void*)buffer, bytes_to_read, context->offset);
	buffer[bytes_to_read] = '\0';
	if(r == 0)
		return;
	if(r == -1) {
		err(1, "Read error in %s\n", context->filename);
	}

	nextStr = (char*)buffer;
	current_token = strsep(&nextStr, "\n");

	if(__builtin_expect(context->offset == 0, false)) { // most of the time this condition fails so branch always predict failure
		read_fileline_to_dataset(context, current_token);
		if(__builtin_expect(context->line_error_flag, false))
			return;
	}

	current_token = strsep(&nextStr, "\n");
	while((next_token = strsep(&nextStr, "\n")) != NULL) {
		read_fileline_to_dataset(context, current_token);
		if(__builtin_expect(context->line_error_flag, false))
			return;
		current_token = next_token;
	}

	off_t lastline_offset = context->offset + (off_t)(current_token - buffer);
	bytes_left = context->filesize - lastline_offset;
	if (bytes_left <= 0) {
		return;
	}
	bytes_to_read = (BUFSIZ < bytes_left)?BUFSIZ:bytes_left;
	nextStr = (char*)buffer;
	r = pread(context->fd, (void*)buffer, bytes_to_read, lastline_offset);
	buffer[bytes_to_read] = '\0';
	if(r == 0)
		return;
	if(r == -1) {
		err(1, "Read error in %s\n", context->filename);
	}

	current_token = strsep(&nextStr, "\n");
	read_fileline_to_dataset(context, current_token);
	// if(context->line_error_flag)
	// 		return;

	return;
}
	
static void 
fill_filechunk_data_int(struct filechunkread_threadcontext_int* context) {
	char *next_token, *current_token, *nextStr;
	off_t bytes_left, bytes_to_read;
	char buffer[FILECHUNK_BUFFSIZE + 1];
	ssize_t r;
	context->output_dataset = NewSet_int();
	context->line_number = 0;

	bytes_left = context->filesize - context->offset;
	bytes_to_read = (FILECHUNK_BUFFSIZE<bytes_left)?FILECHUNK_BUFFSIZE:bytes_left;
	r = pread(context->fd, (void*)buffer, bytes_to_read, context->offset);
	buffer[bytes_to_read] = '\0';
	if(r == 0)
		return;
	if(r == -1) {
		err(1, "Read error in %s\n", context->filename);
	}	

	nextStr = (char*)buffer;
	current_token = strsep(&nextStr, "\n");

	if(__builtin_expect(context->offset == 0, false)) { // most of the time this condition fails so branch always predict failure
		read_fileline_to_dataset_int(context, current_token);
		if(__builtin_expect(context->line_error_flag, false))
			return;
	}

	current_token = strsep(&nextStr, "\n");
	while((next_token = strsep(&nextStr, "\n")) != NULL) {
		read_fileline_to_dataset_int(context, current_token);
		if(__builtin_expect(context->line_error_flag, false))
			return;
		current_token = next_token;
	}

	off_t lastline_offset = context->offset + (off_t)(current_token - buffer);
	bytes_left = context->filesize - lastline_offset;
	if (bytes_left <= 0) {
		return;
	}
	bytes_to_read = (BUFSIZ < bytes_left)?BUFSIZ:bytes_left;
	nextStr = (char*)buffer;
	r = pread(context->fd, (void*)buffer, bytes_to_read, lastline_offset);
	buffer[bytes_to_read] = '\0';
	if(r == 0)
		return;
	if(r == -1) {
		err(1, "Read error in %s\n", context->filename);
	}

	current_token = strsep(&nextStr, "\n");
	read_fileline_to_dataset_int(context, current_token);
	// if(context->line_error_flag)
	// 		return;

	return;
}

void* 
fill_filechunk_data_thread(void* s) {
	struct filechunkread_threadcontext* payload = (struct filechunkread_threadcontext*)s;
	fill_filechunk_data(payload);
	return NULL;
}

void* 
fill_filechunk_data_thread_int(void* s) {
	struct filechunkread_threadcontext_int* payload = (struct filechunkread_threadcontext_int*)s;
	fill_filechunk_data_int(payload);
	return NULL;
}

static struct dataset *
ReadLinkedListSetParallel(const char *n, int column, const char *delim) {
	int fd, max_threads;
	u_int64_t lines_read;
	off_t  filesize, offset;
	struct dataset *s;
	pthread_t* threads;
	struct filechunkread_threadcontext *threadcontexts;
	struct stat st;
	
	stat(n, &st);
	fd = open(n, O_RDONLY);
	filesize = st.st_size;

	if (fd < 0)
		err(1, "Cannot open %s", n);
	else if (filesize < 0) {
		err(1, "Cannot open %s", n);
	}
	max_threads = get_nprocs();
	// init dataset
	s = NewSet();
	lines_read = 0;
	s->name = strdup(n);
	offset = 0;
	threads = (pthread_t*)malloc(sizeof(pthread_t)*max_threads);
	threadcontexts = (struct filechunkread_threadcontext*)malloc(sizeof(struct filechunkread_threadcontext)*max_threads);
	for(int i=0; i < max_threads; i++) {
		threadcontexts[i].fd = fd;
		threadcontexts[i].filesize = filesize;
		threadcontexts[i].column = column;
		threadcontexts[i].row_delim = delim;
		threadcontexts[i].filename = n;
		threadcontexts[i].line_number = 0;
		threadcontexts[i].line_error_flag = false;
		if(__builtin_expect(flag_vt == 1, false)) {
			threadcontexts[i].timestrtod = 0;
			threadcontexts[i].timestrtok = 0;	
		}
	}
	while(offset < filesize) {
		int threads_added;
		for(threads_added = 0; threads_added < max_threads && offset < filesize; offset += FILECHUNK_BUFFSIZE, threads_added++) {
			threadcontexts[threads_added].offset = offset;
			pthread_create(threads + threads_added, NULL, fill_filechunk_data_thread, (void*)(threadcontexts + threads_added));
		}
		for(int i = 0; i < threads_added; i++) {
			pthread_join(threads[i], NULL);
			lines_read += threadcontexts[i].line_number;
			if(threadcontexts[i].line_error_flag) {
				err(2, "Invalid data on line %lu in %s\n", lines_read, n);
			}
			merge_dataset(s, threadcontexts[i].output_dataset);
			free(threadcontexts[i].output_dataset); // to prevent memory leak
		}
	}
	if(__builtin_expect(flag_vt == 1, false)) {
		for(int i=0; i < max_threads; i++) {
			timeStrtod += threadcontexts[i].timestrtod;
			timeStrtok += threadcontexts[i].timestrtok;
		}
	}

	// free resources
	close(fd);
	free(threads);
	free(threadcontexts);
	return s;
}

static struct dataset_int *
ReadLinkedListSetParallel_int(const char *n, int column, const char *delim) {
	int fd, max_threads;
	u_int64_t lines_read;
	off_t  filesize, offset;
	struct dataset_int *s;
	pthread_t* threads;
	struct filechunkread_threadcontext_int *threadcontexts;
	struct stat st;
	
	stat(n, &st);
	fd = open(n, O_RDONLY);
	filesize = st.st_size;

	if (fd < 0)
		err(1, "Cannot open %s", n);
	else if (filesize < 0) {
		err(1, "Cannot open %s", n);
	}
	max_threads = get_nprocs();
	// init dataset
	s = NewSet_int();
	lines_read = 0;
	s->name = strdup(n);
	offset = 0;
	threads = (pthread_t*)malloc(sizeof(pthread_t)*max_threads);
	threadcontexts = (struct filechunkread_threadcontext_int*)malloc(sizeof(struct filechunkread_threadcontext_int)*max_threads);
	for(int i=0; i < max_threads; i++) {
		threadcontexts[i].fd = fd;
		threadcontexts[i].filesize = filesize;
		threadcontexts[i].column = column;
		threadcontexts[i].row_delim = delim;
		threadcontexts[i].filename = n;
		threadcontexts[i].line_number = 0;
		threadcontexts[i].line_error_flag = false;
		if(__builtin_expect(flag_vt == 1, false)) {
			threadcontexts[i].timestrtod = 0;
			threadcontexts[i].timestrtok = 0;	
		}
	}
	while(offset < filesize) {
		int threads_added;
		for(threads_added = 0; threads_added < max_threads && offset < filesize; offset += FILECHUNK_BUFFSIZE, threads_added++) {
			threadcontexts[threads_added].offset = offset;
			pthread_create(threads + threads_added, NULL, fill_filechunk_data_thread_int, (void*)(threadcontexts + threads_added));
		}
		for(int i = 0; i < threads_added; i++) {
			pthread_join(threads[i], NULL);
			lines_read += threadcontexts[i].line_number;
			if(threadcontexts[i].line_error_flag) {
				err(2, "Invalid data on line %lu in %s\n", lines_read, n);
			}
			merge_dataset_int(s, threadcontexts[i].output_dataset);
			free(threadcontexts[i].output_dataset); // to prevent memory leak
		}
	}
	if(__builtin_expect(flag_vt == 1, false)) {
		for(int i=0; i < max_threads; i++) {
			timeStrtod += threadcontexts[i].timestrtod;
			timeStrtok += threadcontexts[i].timestrtok;
		}
	}

	// free resources
	close(fd);
	free(threads);
	free(threadcontexts);
	return s;
}

static struct dataset * ReadLinkedListSetStdin(int column, const char *delim) {
	FILE *f;
	char buf[BUFSIZ], *p, *t, *n;
	struct dataset *s;
	double d;
	int line;
	int i;

	f = stdin;
	n = "<stdin>";
	s = NewSet();
	s->name = strdup(n);
	line = 0;
	while (fgets(buf, sizeof buf, f) != NULL) {
		line++;

		i = strlen(buf);
		if (buf[i-1] == '\n')
			buf[i-1] = '\0';
		
		gettime_ifflagged(&tstart); //Timing start strtok
		char* nextStr = buf;
		for (i = 1, t = strsep(&nextStr, delim);
		     t != NULL && *t != '#';
		     i++, t = strsep(&nextStr, delim)) {
			if (i == column)
				break;
		}
		gettime_ifflagged(&tstop);
		add_elapsed_time(&timeStrtok, &tstart, &tstop); //Store amount of time spent on strtok in seconds
		
		if (t == NULL || *t == '#')
			continue;
		
		gettime_ifflagged(&tstart); //Timing start strtod
		d = vim_strtod(t, &p);
		gettime_ifflagged(&tstop);
		add_elapsed_time(&timeStrtod, &tstart, &tstop); //Store amount of time spent on strtod in seconds
		
		if (p != NULL && *p != '\0')
			err(2, "Invalid data on line %d in %s\n", line, n);
		if (*buf != '\0')
			AddPoint(s, d);
	}
	fclose(f);
	
	return s;	
}

static struct dataset_int * ReadLinkedListSetStdin_int(int column, const char *delim) {
	FILE *f;
	char buf[BUFSIZ], *p, *t, *n;
	struct dataset_int *s;
	int d;
	int line;
	int i;

	f = stdin;
	n = "<stdin>";
	s = NewSet_int();
	s->name = strdup(n);
	line = 0;
	while (fgets(buf, sizeof buf, f) != NULL) {
		line++;

		i = strlen(buf);
		if (buf[i-1] == '\n')
			buf[i-1] = '\0';
		
		gettime_ifflagged(&tstart); //Timing start strtok
		char* nextStr = buf;
		for (i = 1, t = strsep(&nextStr, delim);
		     t != NULL && *t != '#';
		     i++, t = strsep(&nextStr, delim)) {
			if (i == column)
				break;
		}
		gettime_ifflagged(&tstop);
		add_elapsed_time(&timeStrtok, &tstart, &tstop); //Store amount of time spent on strtok in seconds
		
		if (t == NULL || *t == '#')
			continue;
		
		gettime_ifflagged(&tstart); //Timing start strtod
		d = strtol(t, &p, 10); //STRING TO INTEGER
		gettime_ifflagged(&tstop);
		add_elapsed_time(&timeStrtod, &tstart, &tstop); //Store amount of time spent on strtod in seconds
		
		if (p != NULL && *p != '\0')
			err(2, "Invalid data on line %d in %s\n", line, n);
		if (*buf != '\0')
			AddPoint_int(s, d);
	}
	fclose(f);
	
	return s;	
}

static struct dataset *
ReadSet(const char *n, int column, const char *delim)
{
	struct dataset* s;
	if (__builtin_expect(n == NULL || !strcmp(n, "-"), false)) { // we expect most users will not use stdin
		s = ReadLinkedListSetStdin(column, delim);
	}  else {
		s = ReadLinkedListSetParallel(n, column, delim);
	}

	if (s->n < 3) {
		fprintf(stderr,
		    "Dataset %s must contain at least 3 data points\n", n);
		exit (2);
	}
	
	s->points = concatenateList(s);

	gettime_ifflagged(&tstart); // start time

	an_parallel_sort_doubles(s->points, s->n, get_nprocs());
	
	gettime_ifflagged(&tstop);
	add_elapsed_time(&timeSort, &tstart, &tstop);

	return s;
}

static struct dataset_int *
ReadSet_int(const char *n, int column, const char *delim)
{
	struct dataset_int* s;
	if (__builtin_expect(n == NULL || !strcmp(n, "-"), false)) { // we expect most users will not use stdin
		s = ReadLinkedListSetStdin_int(column, delim);
	}  else {
		s = ReadLinkedListSetParallel_int(n, column, delim);
	}

	if (s->n < 3) {
		fprintf(stderr,
		    "Dataset %s must contain at least 3 data points\n", n);
		exit (2);
	}
	
	s->points = concatenateList_int(s);

	gettime_ifflagged(&tstart); // start time

	an_parallel_sort_ints(s->points, s->n, get_nprocs()); //Possible issue?
	// an_qsort_doubles(s->points, s->n);
	// qsort(s->points, s->n, sizeof *s->points, dbl_cmp);
	
	gettime_ifflagged(&tstop);
	add_elapsed_time(&timeSort, &tstart, &tstop);

	return s;
}

static void
usage(char const *whine)
{
	int i;

	fprintf(stderr, "%s\n", whine);
	fprintf(stderr,
	    "Usage: ministat [-C column] [-c confidence] [-d delimiter(s)] [-nsv] [-w width] [file [file ...]]\n");
	fprintf(stderr, "\tconfidence = {");
	for (i = 0; i < NCONF; i++) {
		fprintf(stderr, "%s%g%%",
		    i ? ", " : "",
		    studentpct[i]);
	}
	fprintf(stderr, "}\n");
	fprintf(stderr, "\t-C : column number to extract (starts and defaults to 1)\n");
	fprintf(stderr, "\t-d : delimiter(s) string, default to \" \\t\"\n");
	fprintf(stderr, "\t-i : Set to integer mode (floating point mode is default)\n");
	fprintf(stderr, "\t-n : print summary statistics only, no graph/test\n");
	fprintf(stderr, "\t-q : print summary statistics and test only, no graph\n");
	fprintf(stderr, "\t-s : print avg/median/stddev bars on separate lines\n");
	fprintf(stderr, "\t-v : print verbose timing data\n");
	fprintf(stderr, "\t-w : width of graph/test output (default 74 or terminal width)\n");
	exit (2);
}

int
main(int argc, char **argv)
{
	struct dataset *ds[7];
	struct dataset_int *ds_int[7];
	int nds;
	double a;
	// int a_int;
	const char *delim = " \t";
	char *p;
	int c, i, ci;
	int column = 1;
	int flag_s = 0;
	int flag_n = 0;
	int flag_q = 0;
	int flag_i = 0;
	int termwidth = 74;
	// int flag_vt = 0; //Verbose timing flag
	
	unsigned long long int timeReadSet = 0;
	unsigned long long int timePlot = 0;
	unsigned long long int timeVitals = 0;
	

	if (isatty(STDOUT_FILENO)) {
		struct winsize wsz;

		if ((p = getenv("COLUMNS")) != NULL && *p != '\0')
			termwidth = atoi(p);
		else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz) != -1 &&
			 wsz.ws_col > 0)
			termwidth = wsz.ws_col - 2;
	}

	ci = -1;
	while ((c = getopt(argc, argv, "C:c:d:snqviw:")) != -1)
		switch (c) {
		case 'C':
			column = strtol(optarg, &p, 10);
			if (p != NULL && *p != '\0')
				usage("Invalid column number.");
			if (column <= 0)
				usage("Column number should be positive.");
			break;
		case 'c':
			a = vim_strtod(optarg, &p);
			if (p != NULL && *p != '\0')
				usage("Not a floating point number");
			for (i = 0; i < NCONF; i++)
				if (a == studentpct[i])
					ci = i;
			if (ci == -1)
				usage("No support for confidence level");
			break;
		case 'd':
			if (*optarg == '\0')
				usage("Can't use empty delimiter string");
			delim = optarg;
			break;
		case 'n':
			flag_n = 1;
			break;
		case 'q':
			flag_q = 1;
			break;
		case 's':
			flag_s = 1;
			break;
		case 'v': // **NEW CASE**
			flag_vt = 1;
			break;
		case 'i': // **NEW CASE**
			flag_i = 1;
			break;	
		case 'w':
			termwidth = strtol(optarg, &p, 10);
			if (p != NULL && *p != '\0')
				usage("Invalid width, not a number.");
			if (termwidth < 0)
				usage("Unable to move beyond left margin.");
			break;
		default:
			usage("Unknown option");
			break;
		}
	if (ci == -1)
		ci = 2;
	argc -= optind;
	argv += optind;

	gettime_ifflagged(&tstart); //Timing start
	
	if (argc == 0) {
		if(flag_i == 1)
			ds_int[0] = ReadSet_int("-", column, delim);
		else
			ds[0] = ReadSet("-", column, delim);
		
		nds = 1;
	} else {
		if (argc > (MAX_DS - 1))
			usage("Too many datasets.");
		nds = argc;
		if(flag_i == 1) {
			for (i = 0; i < nds; i++)
				ds_int[i] = ReadSet_int(argv[i], column, delim);
		} else {
			for (i = 0; i < nds; i++) 
				ds[i] = ReadSet(argv[i], column, delim);
		}
	}
	
	gettime_ifflagged(&tstop);
	add_elapsed_time(&timeReadSet, &tstart, &tstop);
	
	gettime_ifflagged(&tstart); //Timing start

	if(flag_i == 1)
		for (i = 0; i < nds; i++)
			printf("%c %s\n", symbol[i+1], ds_int[i]->name);
	else
		for (i = 0; i < nds; i++)
			printf("%c %s\n", symbol[i+1], ds[i]->name);

	if (!flag_n && !flag_q) {
		SetupPlot(termwidth, flag_s, nds);
		for (i = 0; i < nds; i++)
			if(flag_i == 1)
				DimPlot_int(ds_int[i]);
			else
				DimPlot(ds[i]);
		for (i = 0; i < nds; i++)
			if(flag_i == 1)
				PlotSet_int(ds_int[i], i + 1);
			else
				PlotSet(ds[i], i + 1);
		DumpPlot();
	}
	
	gettime_ifflagged(&tstop);
	add_elapsed_time(&timePlot, &tstart, &tstop); //Store amount of time spent on plot in seconds
	
	gettime_ifflagged(&tstart); //Timing start
	
	VitalsHead();
	if(flag_i == 1)
	{
		Vitals_int(ds_int[0], 1);
		for (i = 1; i < nds; i++) {
			Vitals_int(ds_int[i], i + 1);
			if (!flag_n)
				Relative_int(ds_int[i], ds_int[0], ci);
		}
	}
	else {
		Vitals(ds[0], 1);
		for (i = 1; i < nds; i++) {
			Vitals(ds[i], i + 1);
			if (!flag_n)
				Relative(ds[i], ds[0], ci);
		}
	}
	gettime_ifflagged(&tstop);
	add_elapsed_time(&timeVitals, &tstart, &tstop);
	
	unsigned long long int timeTotal = timeReadSet + timePlot + timeVitals + timeSort + timeStrtod + timeStrtok;
	
	if(__builtin_expect(flag_vt == 1, false))
		printf("\nTIMING DATA\n%llu microsecs reading set.\n%llu microsecs plotting.\n%llu microsecs reading and printing vitals.\n%llu microsecs sorting.\n%llu microsecs strtod.\n%llu microsecs strtok.\n%llu microsecs total.\n", timeReadSet, timePlot, timeVitals, timeSort, timeStrtod, timeStrtok, timeTotal);
	
	exit(0);
}
