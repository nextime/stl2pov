/* -*- c -*-
 * Time-stamp: <2009-03-28 09:53:40 rsmith>
 * 
 * stl2pov/stl2pov.c
 * Copyright © 2004--2009 R.F. Smith <rsmith@xs4all.nl>. 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
/* If you don't want debugging and assertions, #define NDEBUG */
/*#define NDEBUG*/
/* If you don't want warning messages, #define QUIET */
/*#define QUIET*/
#include "macros.h"
#include "parse.h"
#include "utils.h"

#ifndef PACKAGE
#error You must define PACKAGE as the name of the application!
#endif

#ifndef VERSION
#error You must define VERSION as the version of the application!
#endif

/* license.c */
extern void print_license(FILE * f);

/* Release allocated memory. */
static void cleanup(void);
/* Run some checks on the assembled vertex-, edge- and facet data. */
static void check_structure(struct stl_reader *r);
/* Print out a smooth_facet mesh. */
static void print_smooth_mesh(struct stl_reader *r);
/* Print out a facet mesh. */
static void print_mesh(struct stl_reader *r);

/*static char *buffer = NULL, *name = NULL;
static size_t len = 0;*/

struct stl_reader *pr = NULL;

int main(int argc, char *argv[])
{
	int smooth = 0;
	int n;
	char *p, *ep;
	float dist;
	char *fmt;
	if (argc < 2) {
		fputs(PACKAGE" "VERSION"\n", stderr);
		fputs("Usage: "PACKAGE" [-s] [-e distance] filename\n\n", 
		      stderr);
		print_license(stderr);
		return 0;
	}
	for (n=1; n<(argc-1); n++) {
		if (strcmp(argv[n], "-s")==0) {
			smooth = 1;
			fprintf(stderr, "Using smooth triangles.\n");
		} else if (strcmp(argv[n], "-e")==0) {
			n++;
			p = ep = argv[n];
			dist = strtof(p, &ep);
			if (p == ep) {
				warning("unreadable distance \"%s\" ignored.\n",
					argv[n]);
			} else {
				epsilon2 = dist*dist;
				fprintf(stderr, "Epsilon = %g units.\n", dist);
			}
		} else {
			warning("unrecognized option \"%s\" ignored.\n",
				argv[n]);
		}
	}
	/* Register cleanup function. It's not a big deal if this
	 * fails. The OS cleans up the memory allocations after the program
	 * anyway. */
	atexit(cleanup);
	/* Perform initializations */
	pr = stl_reader_init(argv[argc-1]);
	if (pr == NULL) {
		return 1;
	}
	if (pr->fmt == 1) {
		fmt = "binary";
	} else {
		fmt = "text";
	}
	fprintf(stderr, "Scanning %s format STL file (%d triangles)\n", fmt, 
		pr->maxnf); 
	
	
 repeat:
	if (stl_reader_step(pr) == 0) {
		fprintf(stderr, "\r%3.0f%% done", (double)pr->pdone);
		fflush(stderr);
		goto repeat;
	}
	fputs("\r100% done\n",stderr);
	/* Perform some checks on the structure, and print the results. */
	check_structure(pr);
	/* Print the mesh itself. */
	fputs("Writing output...\n",stderr);	
	if (smooth) {
		print_smooth_mesh(pr);
	} else {
		print_mesh(pr);
	}
	fputs("\r100% done\n",stderr);	
	fflush(stderr);
	return 0;
}

static void cleanup(void)
{
	if (pr) stl_reader_delete(pr);
}

void check_structure(struct stl_reader *r)
{
	int i, open = 0;
	time_t tm;
	/* Print solid name */
	printf("// Name of the solid: %s\n", r->name);
	/* Print software version, date and time. */
	time(&tm);
	printf("// Generated by "PACKAGE" "VERSION" on %s", ctime(&tm));
	/* Perform some checks on the data. */
	/* For a closed body, all vertices should have refcnt >= 3. */
	for (i=0; i<r->nv; i++) {
		if (r->v[i].refcnt<3) {
			open++;
		}
	}
	if (open==0) {
		printf("// - All vertices used by 3 or more "\
		       "triangles, good.\n");
	}
	open = 0;
	/* For a closed body, all edges should have refcnt == 2. */
	for (i=0; i<r->ne; i++) {
		if (r->e[i].refcnt!=2) {
			open++;
		}
	}
	if (open==0) {
		puts("// - All edges used by 2 triangles, good.");
	} else {
		puts("// ! Warning: some edges are not used by two triangles.");
		puts("//   (There might be visible gaps between triangles.)");
	}
	/* The number of triangles should be even. */
	/*if (r->nf%2 == 0) {
		printf("// - Even number of triangles, good.\n");
	} else {
		puts("// ! Warning: uneven number of triangles.");
		puts("//   (This does not conform to the STL specification.)");
	}*/
	if ((2*r->ne)==(3*r->nf)) {
		puts("// - 2 x #edges == 3 x #triangles, good.");
	} /*else {
		puts("// ! Warning: 2 x #edges != 3 x #triangles.");
		puts("//   (There might be visible gaps between triangles.)");
	} */
	if ((r->nf - r->ne + r->nv)==2) {
		/* We're _assuming_ that the file has only _one_ solid. */
		puts("// - The solid meets Euler's Rule, good.");
	}
}

void print_smooth_mesh(struct stl_reader *r)
{
	int i, k1, k2, k3, r1, r2, r3;
	float n1[3], n2[3], n3[3];
	double p;
	/* Print the mesh itself. */
	printf("#declare m_%s = mesh {\n", r->name);

	for (i=0; i<r->nf; i++) {
		k1 = r->f[i].vertex[0];
		k2 = r->f[i].vertex[1];
		k3 = r->f[i].vertex[2];
		r1 = calc_vertex_normal(r, i, k1, n1);
		r2 = calc_vertex_normal(r, i, k2, n2);
		r3 = calc_vertex_normal(r, i, k3, n3);
		if (r1||r2||r3) {
			printf("  smooth_triangle { // #%d\n", i+1);
			printf("    <%g, %g, %g>, <%g, %g, %g>,\n",
			       (double)r->v[k1].xyz[0], 
			       (double)r->v[k1].xyz[1], 
			       (double)r->v[k1].xyz[2],
			       (double)n1[0], (double)n1[1], (double)n1[2]);
			printf("    <%g, %g, %g>, <%g, %g, %g>,\n",
			       (double)r->v[k2].xyz[0], 
			       (double)r->v[k2].xyz[1], 
			       (double)r->v[k2].xyz[2],
			       (double)n2[0], (double)n2[1], (double)n2[2]);
			printf("    <%g, %g, %g>, <%g, %g, %g>\n",
			       (double)r->v[k3].xyz[0], 
			       (double)r->v[k3].xyz[1], 
			       (double)r->v[k3].xyz[2],
			       (double)n3[0], (double)n3[1], (double)n3[2]);
		} else {
			printf("  triangle { // #%d\n", i+1);
			printf("    <%g, %g, %g>,\n",
			       (double)r->v[k1].xyz[0], 
			       (double)r->v[k1].xyz[1], 
			       (double)r->v[k1].xyz[2]);
			printf("    <%g, %g, %g>,\n",
			       (double)r->v[k2].xyz[0], 
			       (double)r->v[k2].xyz[1], 
			       (double)r->v[k2].xyz[2]);
			printf("    <%g, %g, %g>\n",
			       (double)r->v[k3].xyz[0], 
			       (double)r->v[k3].xyz[1], 
			       (double)r->v[k3].xyz[2]);
		}
		puts("  }");
		p= ((double)i)/r->nf*100;
		fprintf(stderr, "\r%3.0f%% done", p);
		fflush(stderr);
	}
	printf("} // end of mesh m_%s\n", r->name);
}

void print_mesh(struct stl_reader *r)
{
	int i, k;
	double p;
	/* Print the mesh itself. */
	printf("mesh {\n", r->name);
	for (i=0; i<r->nf; i++) {
		printf("  triangle { // #%d\n", i+1);
		k = r->f[i].vertex[0];
		if (r->v[k].xyz[0] > xyz_max[0]) { xyz_max[0] = r->v[k].xyz[0]; } else { if (r->v[k].xyz[0] < xyz_min[0]) { xyz_min[0] = r->v[k].xyz[0]; } }
		if (r->v[k].xyz[1] > xyz_max[1]) { xyz_max[1] = r->v[k].xyz[1]; } else { if (r->v[k].xyz[1] < xyz_min[1]) { xyz_min[1] = r->v[k].xyz[1]; } }
		if (r->v[k].xyz[2] > xyz_max[2]) { xyz_max[2] = r->v[k].xyz[2]; } else { if (r->v[k].xyz[2] < xyz_min[2]) { xyz_min[2] = r->v[k].xyz[2]; } }
		printf("    <%g, %g, %g>,\n",
		       (double)r->v[k].xyz[0], (double)r->v[k].xyz[1], 
		       (double)r->v[k].xyz[2]);
		k = r->f[i].vertex[1];
		if (r->v[k].xyz[0] > xyz_max[0]) { xyz_max[0] = r->v[k].xyz[0]; } else { if (r->v[k].xyz[0] < xyz_min[0]) { xyz_min[0] = r->v[k].xyz[0]; } }
		if (r->v[k].xyz[1] > xyz_max[1]) { xyz_max[1] = r->v[k].xyz[1]; } else { if (r->v[k].xyz[1] < xyz_min[1]) { xyz_min[1] = r->v[k].xyz[1]; } }
		if (r->v[k].xyz[2] > xyz_max[2]) { xyz_max[2] = r->v[k].xyz[2]; } else { if (r->v[k].xyz[2] < xyz_min[2]) { xyz_min[2] = r->v[k].xyz[2]; } }
		printf("    <%g, %g, %g>,\n",
		       (double)r->v[k].xyz[0], (double)r->v[k].xyz[1], 
		       (double)r->v[k].xyz[2]);
		k = r->f[i].vertex[2];
		if (r->v[k].xyz[0] > xyz_max[0]) { xyz_max[0] = r->v[k].xyz[0]; } else { if (r->v[k].xyz[0] < xyz_min[0]) { xyz_min[0] = r->v[k].xyz[0]; } }
		if (r->v[k].xyz[1] > xyz_max[1]) { xyz_max[1] = r->v[k].xyz[1]; } else { if (r->v[k].xyz[1] < xyz_min[1]) { xyz_min[1] = r->v[k].xyz[1]; } }
		if (r->v[k].xyz[2] > xyz_max[2]) { xyz_max[2] = r->v[k].xyz[2]; } else { if (r->v[k].xyz[2] < xyz_min[2]) { xyz_min[2] = r->v[k].xyz[2]; } }
		printf("    <%g, %g, %g>\n",
		       (double)r->v[k].xyz[0], (double)r->v[k].xyz[1], 
		       (double)r->v[k].xyz[2]);
		puts("  }");
		p= ((double)i)/r->nf*100;
		fprintf(stderr, "\r%3.0f%% done", p);
		fflush(stderr);
	}
	printf("    texture {\n");
	printf("      pigment { color rgb<0.9, 0.9, 0.9> }\n");
   printf("      finish { ambient 0.2 diffuse 0.7 }\n");
   printf(" }\n");

	printf("} // end of mesh m_%s\n", r->name);

	printf("  global_settings { ambient_light rgb<1, 1, 1> }\n");
	printf("  camera {\n");
	printf("    location <%g, %g, %g>\n", (double)xyz_max[0] * 1.5, (double)xyz_max[1] * 1.5,	(double)xyz_max[2] * 1.5);
	printf("    look_at <%g, %g, %g>\n", ((double)xyz_max[0] - (double)xyz_min[0]) / 2 + (double)xyz_min[0], ((double)xyz_max[1] - (double)xyz_min[1]) / 2 + (double)xyz_min[1], ((double)xyz_max[2] - (double)xyz_min[2]) / 2 + (double)xyz_min[2]);
	printf("  }\n");
	printf("  light_source { <%g, %g, %g> color rgb<1, 1, 1> }\n", (double)xyz_max[0] * 1.5, (double)xyz_max[1] * 1.5,	(double)xyz_max[2] * 1.5);
}
/* EOF stl2pov.c */
