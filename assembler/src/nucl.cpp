/*
 * nucl.cpp
 *
 *  Created on: 01.03.2011
 *      Author: vyahhi
 */

#include "nucl.hpp"
#include <cassert>

char complement(char c) { // 0123 -> 3210
	assert(c >= 0);
	assert(c < 4);
	return c ^ 3;
}

char nucl(char c) { // 01234 -> ACGT
	assert(c >= 0);
	assert(c < 4);
	switch(c) {
		case 0: return 'A';
		case 1: return 'C';
		case 2: return 'G';
		case 3: return 'T';
		default: return 'N'; // never happens
	}
}

char denucl(char c) { // ACGT -> 0123
	assert(is_nucl(c));
	switch(c) {
		case 'A': return 0;
		case 'C': return 1;
		case 'G': return 2;
		case 'T': return 3;
		default: return -1; // never happens
	}
}

bool is_nucl(char c) {
	return (c == 'A' || c == 'C' || c == 'G' || c == 'T');
}


