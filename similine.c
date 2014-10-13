/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2013-2014 Eddie Lebow
 */

/*TODO
 * support stdin input (can't use mmap())
 * when -w and -c are both specified, keep_space is false for some reason. Probably using libpopt incorrectly.
 * support alternative algorithms. Levenstein, longest common subsequence.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <error.h>
#include <popt.h>
#include <stdbool.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

bool space_sens = false, case_sens = false, show_blanks = false;
unsigned int lines_size = 60;
char **lines = NULL;
unsigned int *line_lengths = NULL;

struct bigram {
	char chars[2];
	struct bigram *next;
};

unsigned int make_list(const char *z, const unsigned int zlen, struct bigram *z_head, struct bigram *z_current, struct bigram *z_previous) {
	//build a linked list of bigrams, skipping spaces
	unsigned int n = 0, z_bigrams = 0;
	//while (z[n+1] != '\n' && z[n+1] != '\0') {	//foreach char in the line except the last one
	while (n < zlen) {	//foreach char in z up to the length of z
		if (!space_sens && (z[n] == ' ' || z[n+1] == ' ')) {	//if space sensitive and we found a space
			n++;	//advance the read pointer
			continue;
		}
		if (z_current == NULL) {	//first element
			z_current = z_head;
		} else {							//not-first element
			z_current = (struct bigram*) malloc(sizeof(struct bigram));
			z_previous->next = z_current;
		}
		z_current->next = NULL;
		if (!case_sens) {
			z_current->chars[0] = (char)tolower(z[n]);		//there can't be data loss casting to char because z[n] is a char
			z_current->chars[1] = (char)tolower(z[n+1]);
		} else {
			z_current->chars[0] = z[n];
			z_current->chars[1] = z[n+1];
		}
		z_previous = z_current;
		z_bigrams++;
		n++;
	}
	return z_bigrams;
}

void free_list(struct bigram *z) {
	struct bigram *z_current = z;
	while (z_current != NULL) {
		struct bigram *z_temp = z_current->next;
		free(z_current);
		z_current = z_temp;
	}
}

double dice(const char *x, const unsigned int xlen, const char *y, const unsigned int ylen) {
	//calculate Sørensen–Dice coefficient. Pseudocode taken from Wikipedia.
	struct bigram x_head = {"", NULL}, *x_current = NULL, *x_previous = NULL;
	struct bigram y_head = {"", NULL}, *y_current = NULL, *y_previous = NULL;

	unsigned int n = 0, x_bigrams = 0, y_bigrams = 0;

	if (x[0] == '\0' || y[0] == '\0')
		return 0;

	x_bigrams = make_list(x, xlen, &x_head, x_current, x_previous);
	y_bigrams = make_list(y, ylen, &y_head, y_current, y_previous);

	n = 0;
	x_current = &x_head;
	while (x_current != NULL) {	//foreach x pair
		y_current = &y_head;
		while (y_current != NULL) {	//foreach y pair
			if (y_current->chars[0] == x_current->chars[0] && y_current->chars[1] == x_current->chars[1]) {	//both bigram match
				y_current->chars[0] = 0;
				n+=2;
				break;
			}
			y_current=y_current->next;
		}
		x_current=x_current->next;
	}

	free_list(x_head.next);
	free_list(y_head.next);

	return ((double)n/(x_bigrams+y_bigrams));
}

unsigned int lines_to_array(char *ls){//, char *lines[], unsigned int lengths[]) {
	//Takes a mmap'd file and populates an array of pointers to the beginning of each line and one to the length of each line
	//returns number of lines

	unsigned int i = 0, ii = 0, n = 0;

	lines[0] = &ls[0];	//the first line starts at position 0 in the file

	for ( ; ls[i+1] != '\0'; i++) {		//foreach character in the file until we reach NULL
		if (ls[i] == '\n') {
			if (n+1 == lines_size) {	//if we ran out of space in the lines arrays
				lines_size *= 2;
				if (!(lines = realloc(lines, sizeof(char*)*lines_size))) {
					fprintf(stderr, "Can't allocate memory: %s\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				if (!(line_lengths = realloc(line_lengths, sizeof(unsigned int)*lines_size))) {
					fprintf(stderr, "Can't allocate memory: %s\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
			}

			line_lengths[n] = ii;	//store line length
			ii = 0;						//reset line_length counter
			n++;							//increment line counter
			lines[n] = &ls[i+1];		//start the next line
		} else {
			ii++;
		}
	}

	line_lengths[n] = ii;			//the final line

	return n+1;
}

void process(const char *f, double threshold) {
	printf("#%s\n", f);

	struct stat sb;
	int infile = open(f, O_RDONLY);
	fstat(infile, &sb);	//get the file size (among other things)
	void *ls = mmap(NULL, (size_t)sb.st_size, PROT_READ, MAP_PRIVATE, infile, 0);

	//we want to operate line by line, so we'll need two things...
	lines = malloc(sizeof(char*)*lines_size);	//...a pointer to the beginning of each line...
	line_lengths = malloc(sizeof(unsigned int)*lines_size);	//...and the length of each line
	unsigned int num_lines = lines_to_array(ls);	//populate those two arrays

	for (unsigned int i = 0; i < num_lines; i++) {
		for (unsigned int ii = i+1; ii < num_lines; ii++) {
			if (!show_blanks && (line_lengths[i] == 0 || line_lengths[i] == 0))	//don't compare blank lines to anything
				continue;
			double c = dice(lines[i], line_lengths[i], lines[ii], line_lengths[ii]);
			if (c > threshold) {
				printf("%f\n%d:", c, i+1);								//filename, x_linenum
				for (unsigned int iii = 0; iii < line_lengths[i]; iii++)
					putchar(lines[i][iii]);								//x
				printf("\n%d:", ii+1);									//y_linenum
				for (unsigned int iii = 0; iii < line_lengths[ii]; iii++)
					putchar(lines[ii][iii]);							//y
				printf("\n");
				/*
				char *x = malloc(line_lengths[i]+1);
				char *y = malloc(line_lengths[ii]+1);
				strncpy(x, lines[i], line_lengths[i]);
				x[line_lengths[i]] = '\0';
				strncpy(y, lines[ii], line_lengths[ii]);
				y[line_lengths[ii]] = '\0';
				printf("%f\n%d:%s\n%d:%s\n", c, i+1, x, ii+1, y);
				free(x);
				free(y);*/
			}
		}
	}

	free(lines);
	free(line_lengths);

	return;
}

int main(int argc, const char *argv[]) {
	double threshold = 0.75;
	int stop_after = -1;

	unsigned int incount = 0;
	char **inputs;
	const char **fopt = NULL;
	poptContext optCon;

	struct poptOption optionsTable[] = {
		{"stop-after",				's',	POPT_ARG_INT,		&stop_after,	0,			"maximum number of lines to process", "number"},
		{"threshold",				't',	POPT_ARG_DOUBLE,	&threshold,		0,			"similarity threshold (default 0.75)", "double"},
		{"whitespace-sensitive",'w',	POPT_ARG_NONE,		&space_sens,	true,		"use whitespace-sensitive comparison (do not collapse whitespace)", NULL},
		{"case-sensitive",		'c',	POPT_ARG_NONE,		&case_sens,		true,		"use case-sensitive comparison", NULL},
		{"show-blank-lines",		'b',	POPT_ARG_NONE,		&show_blanks,	true,		"show comparisons with blank lines", NULL},
		POPT_AUTOHELP
		{NULL, 0, 0, NULL, 0, NULL, NULL}
	};

	optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "[files]...");

	int c;
	while ((c = poptGetNextOpt(optCon)) >= 0) {	//foreach option with a non-NULL second parameter
		switch (c) {
			case 's':
				if (stop_after <= 0)
					fprintf(stderr, "Warning: --stop-after is not a positive integer.\n");
				break;
		}
	}
	if (c < -1) {	// an error occurred during option processing
		fprintf(stderr, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));
		return 1;
	}

	fopt = poptGetArgs(optCon);	//get all the remaining arguments
	if (fopt == NULL) {
		fprintf(stderr, "At least one input file is required.\n");
		exit(EXIT_FAILURE);
	} else {		//assume each of the remaining arguments is the name of an input file
		for (unsigned int i = 0; fopt[i] != NULL; i++)
			incount++;	//count them
		inputs = malloc(sizeof(char*)*incount);
		for (unsigned int i = 0; i < incount; i++) {
			if ((inputs[i] = realpath(fopt[i], NULL)) == NULL) {	//try to add it to the array
				fprintf(stderr, "Can't canonicalize path '%s': %s\n", fopt[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}

	poptFreeContext(optCon);

	for (unsigned int i = 0; i < incount; i++) {
		process(inputs[i], threshold);
		free(inputs[i]);
	}

	free(inputs);

	return 0;
}
