#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cassert>
#include <cstdint>

#include "alignments.hpp"
#include "../data/data.hpp"

Alignments::Alignments(std::string reference, std::string uLongRead, std::string cLongRead)
/* Constructor for general reads class - is the parent of UntrimmedAlignments and TrimmedAlignments */
{
	ref = reference;
	ulr = uLongRead;
	clr = cLongRead;
	createMatrix();
}

Alignments::Alignments(const Alignments &reads)
/* Copy constructor */
{
	// First, copy all member fields
	clr = reads.clr;
	ulr = reads.ulr;
	ref = reads.ref;
	rows = 0;
	columns = 0;
	matrix = NULL;
}

Alignments::~Alignments()
/* Delete the matrix when calling the destructor */
{
	deleteMatrix();
}

void Alignments::reset(std::string reference, std::string uLongRead, std::string cLongRead)
/* Resets variables to new values and deletes and recreates matrix given new information */
{
	// Resets in case of need to reassign values of object
	ref = reference;
	ulr = uLongRead;
	clr = cLongRead;
	deleteMatrix();
	createMatrix();
}

std::string Alignments::getClr()
/* Returns optimal cLR alignment ready to be written in 3-way MAF file */
{
	return clr;
}


std::string Alignments::getUlr()
/* Returns optimal uLR alignment ready to be written in 3-way MAF file */
{
	return ulr;
}

std::string Alignments::getRef()
/* Returns optimal ref alignment ready to be written in 3-way MAF file */
{
	return ref;
}

void Alignments::createMatrix()
/* Preconstruct the matrix */
{
	std::string cleanedClr = clr; 
	cleanedClr.erase(std::remove(cleanedClr.begin(), cleanedClr.end(), ' '), cleanedClr.end());

	rows = cleanedClr.length() + 1;
	columns = ulr.length() + 1;
 
	try {
		matrix = new int*[rows];
	} catch( std::bad_alloc& ba ) {
		std::cout << "Memory allocation failed; unable to create DP matrix.\n";
	}
	
	for (int64_t rowIndex = 0; rowIndex < rows; rowIndex++) {
		try {
			matrix[rowIndex] = new int[columns];
		} catch( std::bad_alloc& ba ) {
			std::cout << "Memory allocation failed; unable to create DP matrix.\n";
		}
	}
}

void Alignments::deleteMatrix()
/* Delete the matrix allocated in the heap */
{
	for (int64_t rowIndex = 0; rowIndex < rows; rowIndex++) {
		if (matrix[rowIndex] != NULL) {
			delete matrix[rowIndex];
		} 	
	}

	if (matrix != NULL) {
		delete matrix;
	}
}

int64_t Alignments::cost(char refBase, char cBase)
/* Cost function for dynamic programming algorithm */
{
	if ( islower(cBase) ) {
		return 0;
	} else if ( toupper(refBase) == cBase ) {
		return 0;
	} else {
		// Ideally, in an alignment between cLR and ref we want to minimize the number of discrepancies
		// as much as possible, so if both bases are different, we assign a cost of 2.
		return 2;
	}
}

void Alignments::printMatrix()
/* Print64_t the matrix */
{
	int64_t infinity = std::numeric_limits<int>::max();
	for (int64_t rowIndex = 0; rowIndex < rows; rowIndex++) {
		for (int64_t columnIndex = 0; columnIndex < columns; columnIndex++) {
			int64_t val = matrix[rowIndex][columnIndex];
			if (val == infinity) {
				std::cout << "- ";
			} else {
				std::cout << val << " ";
			}
		}
		std::cout << "\n";
	}
}

/*--------------------------------------------------------------------------------------------------------*/

UntrimmedAlignments::UntrimmedAlignments(std::string reference, std::string uLongRead, std::string cLongRead)
	: Alignments(reference, uLongRead, cLongRead)
/* Constructor */
{ initialize(); }

void UntrimmedAlignments::reset(std::string reference, std::string uLongRead, std::string cLongRead)
/* Resets variables to new values and deletes and recreates matrix given new information */
{ 
	Alignments::reset(reference, uLongRead, cLongRead);
	initialize(); 
}

bool UntrimmedAlignments::checkIfEndingLowerCase(int64_t cIndex)
/* Determine if we're at an ending lower case i.e. if the current base
 * in cLR is lowercase and the following base is uppercase or the
 * current base is lowercase and the last base in the sequene.
 */
{
	if ( (cIndex < clr.length() - 1 && islower(clr[cIndex]) && isupper(clr[cIndex+1])) || 
		(cIndex == clr.length() - 1 && islower(clr[cIndex])) ) {
		return true;
	} else {
		return false;
	}
}

void UntrimmedAlignments::initialize()
/* Given cLR, uLR and ref sequences, construct the DP matrix for the optimal alignments. 
 * Requires these member variables to be set before use. */
{
	int64_t cIndex;
	int64_t urIndex;
	int64_t keep;
	int64_t substitute;
	int64_t insert;
	int64_t deletion;
	int64_t infinity = std::numeric_limits<int>::max();

	// Set the base cases for the DP matrix
	for (int64_t rowIndex = 0; rowIndex < rows; rowIndex++) {
		matrix[rowIndex][0] = rowIndex;	
	}
	for (int64_t columnIndex = 1; columnIndex < columns; columnIndex++) {
		matrix[0][columnIndex] = columnIndex;
	}

	// Find the optimal edit distance such that all uncorrected segments of clr are aligned with uncorrected
 	// portions of ulr. 
	for (int64_t rowIndex = 1; rowIndex < rows; rowIndex++) {
		for (int64_t columnIndex = 1; columnIndex < columns; columnIndex++) {
			cIndex = rowIndex - 1;
			urIndex = columnIndex -  1;

			// Determine if the current letter in clr is lowercase and is followed by an upper case letter
			// i.e. if the current letter in clr is an ending lowercase letter
			/*
			if ( (cIndex < clr.length() - 1 && islower(clr[cIndex]) && isupper(clr[cIndex+1])) || 
				(cIndex == clr.length() - 1 && islower(clr[cIndex])) ) {
				isEndingLC = true;	
			} else {
				isEndingLC = false;
			}
			*/
			bool isEndingLC = checkIfEndingLowerCase(cIndex);

			if (isEndingLC) {
				// If both letters are the same, we can either keep both letters or deletion the one from
				// clr. If they are different, we can't keep both so we can only consider deleting the
				// one from clr.
				if ( toupper(ulr[urIndex]) == toupper(clr[cIndex]) ) {
					keep = std::abs( matrix[rowIndex-1][columnIndex-1] + cost(ref[urIndex], clr[cIndex]) );
					deletion = std::abs( matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-') );
					matrix[rowIndex][columnIndex] = std::min(keep, deletion); 
				} else {
					matrix[rowIndex][columnIndex] = std::abs( matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-') ); 
				}
			} else if (islower(clr[cIndex])) {
				if ( toupper( ulr[urIndex] ) == toupper( clr[cIndex] ) ) {
					// Keep the characters if they are the same
					matrix[rowIndex][columnIndex] = std::abs( matrix[rowIndex-1][columnIndex-1] + cost(ref[urIndex], clr[cIndex]) );
				} else if (ulr[urIndex] == '-') {
					// If uLR has a dash, the optimal solution is just to call a deletion.
					matrix[rowIndex][columnIndex] = matrix[rowIndex][columnIndex-1]; //Zero cost deletion
				} else {
					// Setting the position in the matrix to infinity ensures that we can never
					// find an alignment where the uncorrected segments are not perfectly aligned.
					matrix[rowIndex][columnIndex] = infinity;
				}
			} else {
				// Usual Levenshtein distance equations.
				deletion = std::abs( matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-') );
				insert = std::abs( matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]) );
				substitute = std::abs( matrix[rowIndex-1][columnIndex-1] + cost(ref[urIndex], clr[cIndex]) );
				matrix[rowIndex][columnIndex] = std::min( deletion, std::min(insert, substitute) ); 
			}
		}		
	}
	findAlignments();
}

void UntrimmedAlignments::findAlignments()
/* Backtracks through the DP matrix to find the optimal alignments. 
 * Follows same schema as the DP algorithm. */
{
	std::string clrMaf = "";
	std::string ulrMaf = "";
	std::string refMaf = "";
	int64_t rowIndex = rows - 1;
	int64_t columnIndex = columns - 1;
	int64_t cIndex;
	int64_t urIndex;
	int64_t insert;
	int64_t deletion;
	int64_t substitute;
	int64_t currentCost;
	bool isEndingLC;
	int64_t infinity = std::numeric_limits<int>::max();

	// Follow the best path from the bottom right to the top left of the matrix.
	// This is equivalent to the optimal alignment between ulr and clr.
	// The path we follow is restricted to the conditions set when computing the matrix,
	// i.e. we can never follow a path that the edit distance equations do not allow.
	while (rowIndex > 0 || columnIndex > 0) {
		/*
		std::cout << "rowIndex == " << rowIndex << "\n";
		std::cout << "columnIndex == " << columnIndex << "\n";
		std::cout << "Before\n";
		std::cout << "clrMaf == " << clrMaf << "\n";
		std::cout << "ulrMaf == " << ulrMaf << "\n";
		std::cout << "refMaf == " << refMaf << "\n";
		*/

		urIndex = columnIndex - 1;
		cIndex = rowIndex - 1;
		currentCost = matrix[rowIndex][columnIndex];

		// Set the costs of the different operations, 
		// ensuring we don't go out of bounds of the matrix.
		if (rowIndex > 0 && columnIndex > 0) {
			deletion = std::abs( matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-') );
			insert = std::abs( matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]) );
			substitute = std::abs( matrix[rowIndex-1][columnIndex-1] + cost(ref[urIndex], clr[cIndex]) );	
		} else if (rowIndex <= 0 && columnIndex > 0) {
			deletion = std::abs( matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-') );
			insert = infinity;
			substitute = infinity;
		} else if (rowIndex > 0 && columnIndex <= 0) {
			deletion = infinity;
			insert = std::abs( matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]) );
			substitute = infinity;
		} 

		// Make sure we follow the same path as dictated by the edit distance equations. 
		// Determine if the current letter in clr is lowercase and is followed by an upper case letter
		// i.e. if the current letter in clr is an ending lowercase letter
		/*
		if ( (cIndex < clr.length() - 1 && islower(clr[cIndex]) && isupper(clr[cIndex+1])) || 
			(cIndex == clr.length() - 1 && islower(clr[cIndex])) ) {
			isEndingLC = true;	
		} else {
			isEndingLC = false;
		}
		*/
		bool isEndingLC = checkIfEndingLowerCase(cIndex);

		if (rowIndex == 0 || columnIndex == 0) {
				//std::cout << "Path 6\n";
				if (rowIndex == 0) {
					//std::cout << "Deletion\n";
					clrMaf = '-' + clrMaf;
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					columnIndex--;
				} else {
					//std::cout << "Insertion\n";
					clrMaf = clr[cIndex] + clrMaf;
					ulrMaf = '-' + ulrMaf;
					refMaf = '-' + refMaf;
					rowIndex--;
				}
		} else if (isEndingLC) {
			if ( toupper( ulr[urIndex] ) == toupper( clr[cIndex] ) ) {
				//std::cout << "Path 1\n";
				if (deletion == currentCost) {
					//std::cout << "Deletion\n";
					clrMaf = '-' + clrMaf;
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					columnIndex--;
				} else if (substitute == currentCost) {
					//std::cout << "Substitution\n";
					clrMaf = clr[cIndex] + clrMaf;
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					rowIndex--;
					columnIndex--;
				} else {
					std::cout << "ERROR CODE 1: No paths found. Terminating backtracking.\n";	
					std::cout << "cIndex is " << cIndex << "\n";
					std::cout << "urIndex is " << urIndex << "\n";
					rowIndex = 0;
					columnIndex = 0;
				}
			} else {
				//std::cout << "Path 2\n";
				if (deletion == currentCost) {
					//std::cout << "Deletion\n";
					clrMaf = '-' + clrMaf;
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					columnIndex--;
				} else {
					std::cout << "ERROR CODE 2: No paths found. Terminating backtracking.\n";
					std::cout << "cIndex is " << cIndex << "\n";
					std::cout << "urIndex is " << urIndex << "\n";
					rowIndex = 0;
					columnIndex = 0;
				}
			}
		} else if (islower(clr[cIndex])) {
			if ( toupper( ulr[urIndex] ) == toupper( clr[cIndex] ) ) {
				//std::cout << "Path 3\n";
				if (substitute == currentCost) {
					//std::cout << "Substitution\n";
					clrMaf = clr[cIndex] + clrMaf;	
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					rowIndex--;
					columnIndex--;
				} else {
					std::cout << "ERROR CODE 3: No paths found. Terminating backtracking.\n";
					std::cout << "cIndex is " << cIndex << "\n";
					std::cout << "urIndex is " << urIndex << "\n";
					rowIndex = 0;
					columnIndex = 0;
				}
			} else if (ulr[urIndex] == '-') {
				//std::cout << "Path 4\n";
				deletion = matrix[rowIndex][columnIndex-1];
				if (deletion == currentCost) {
					//std::cout << "Deletion\n";
					clrMaf = '-' + clrMaf;
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					columnIndex--;
				} else {
					std::cout << "ERROR CODE 4: No paths found. Terminating backtracking.\n";
					std::cout << "cIndex is " << cIndex << "\n";
					std::cout << "urIndex is " << urIndex << "\n";
					rowIndex = 0;
					columnIndex = 0;
				}
			} else {
				std::cout << "ERROR CODE 5: No paths found. Terminating backtracking.\n";
				std::cout << "cIndex is " << cIndex << "\n";
				std::cout << "urIndex is " << urIndex << "\n";
				rowIndex = 0;
				columnIndex = 0;
			}
		} else {
			//std::cout << "Path 5\n";
			if (deletion == currentCost) {
				//std::cout << "Deletion\n";
				clrMaf = '-' + clrMaf;
				ulrMaf = ulr[urIndex] + ulrMaf;
				refMaf = ref[urIndex] + refMaf;
				columnIndex--;
			} else if (insert == currentCost) {
				//std::cout << "Insertion\n";
				clrMaf = clr[cIndex] + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
				rowIndex--;
			} else if (substitute == currentCost) {
				//std::cout << "Substitution\n";
				clrMaf = clr[cIndex] + clrMaf;
				ulrMaf = ulr[urIndex] + ulrMaf;
				refMaf = ref[urIndex] + refMaf;
				rowIndex--;
				columnIndex--;
			} else {
				std::cout << "ERROR CODE 6: No paths found. Terminating backtracking.\n";
				std::cout << "cIndex is " << cIndex << "\n";
				std::cout << "urIndex is " << urIndex << "\n";
				rowIndex = 0;
				columnIndex = 0;
			}
		} 		
		/*
		std::cout << "After\n";
		std::cout << "clrMaf == " << clrMaf << "\n";
		std::cout << "ulrMaf == " << ulrMaf << "\n";
		std::cout << "refMaf == " << refMaf << "\n\n";
		*/
	}

	clr = clrMaf;
	ulr = ulrMaf;
	ref = refMaf;
}

/* --------------------------------------------------------------------------------------------- */

TrimmedAlignments::TrimmedAlignments(std::string reference, std::string uLongRead, std::string cLongRead)
	: Alignments(reference, uLongRead, cLongRead)
/* Constructor - is a child class of Alignments */
{ initialize(); }

void TrimmedAlignments::reset(std::string reference, std::string uLongRead, std::string cLongRead)
/* Resets the alignment object to be used again */
{
	Alignments::reset(reference, uLongRead, cLongRead);
	lastBaseIndices.clear();
 	initialize(); 
}

void TrimmedAlignments::initialize()
/* Create the DP matrix similar to generic alignment object */
{
	// Split the clr into its corrected parts
	std::vector< std::string > trimmedClrs = split(clr);
	// Remove spaces in clr
	clr.erase(std::remove(clr.begin(), clr.end(), ' '), clr.end());

	rows = clr.length() + 1;
	columns = ref.length() + 1;

	int64_t cIndex;
	int64_t urIndex;
	int64_t substitute;
	int64_t insert;
	int64_t deletion;

	int64_t lastBaseIndex = -1;
	bool isLastBase;
	
	// Record the indices of the first bases of all the reads
	for (int64_t index = 0; index < trimmedClrs.size(); index++) {
		lastBaseIndex = lastBaseIndex + trimmedClrs.at(index).length();
		lastBaseIndices.push_back(lastBaseIndex);	
	}

	// Set the base cases for the DP matrix
	for (int64_t rowIndex = 0; rowIndex < rows; rowIndex++) {
		matrix[rowIndex][0] = rowIndex;	
	}
	for (int64_t columnIndex = 1; columnIndex < columns; columnIndex++) {
		matrix[0][columnIndex] = 0;
	}

	// Find the minimal edit distances
	for (int64_t rowIndex = 1; rowIndex < rows; rowIndex++) {
		for (int64_t columnIndex = 1; columnIndex < columns; columnIndex++) {
			cIndex = rowIndex - 1;
			urIndex = columnIndex - 1;

			// Check if cIndex is the first base of a read
			if (std::find( lastBaseIndices.begin(), lastBaseIndices.end(), cIndex ) != lastBaseIndices.end()) {
				isLastBase = true;
			} else {
				isLastBase = false;
			} 

			if (isLastBase) {
				deletion = matrix[rowIndex][columnIndex-1];
			} else {
				deletion = matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-');
			}	

			insert = matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]);
			substitute = matrix[rowIndex-1][columnIndex-1] + cost(clr[cIndex], ref[urIndex]);
			matrix[rowIndex][columnIndex] = std::min( deletion, std::min( insert, substitute ) );
		}
	}

	findAlignments();
}

void TrimmedAlignments::findAlignments()
/* Construct the optimal alignments between the trimmed corrected long reads, 
 * uncorrected long read and reference sequence. These algorithm indicates the boundaries
 * of the original trimmed long reads by placing X's immediately left and right of the boundaries
 * of the trimmed long reads.
*/
{
	std::string clrMaf = "";
	std::string ulrMaf = "";
	std::string refMaf = "";
	int64_t rowIndex = rows - 1;
	int64_t columnIndex = columns - 1;
	int64_t cIndex;
	int64_t urIndex;
	int64_t insert;
	int64_t deletion;
	int64_t substitute;
	int64_t currentCost;
	int64_t infinity = std::numeric_limits<int>::max();
	bool isLastBase;
	bool firstDeletion = false;

	// Follow the best path from the bottom right to the top left of the matrix.
	// This is equivalent to the optimal alignment between ulr and clr.
	// The path we follow is restricted to the conditions set when computing the matrix,
	// i.e. we can never follow a path that the edit distance equations do not allow.
	while (rowIndex > 0 || columnIndex > 0) {
		/*
		std::cout << "rowIndex == " << rowIndex << "\n";
		std::cout << "columnIndex == " << columnIndex << "\n";
		std::cout << "Before\n";
		std::cout << "clrMaf == " << clrMaf << "\n";
		std::cout << "ulrMaf == " << ulrMaf << "\n";
		std::cout << "refMaf == " << refMaf << "\n";
		*/

		urIndex = columnIndex - 1;
		cIndex = rowIndex - 1;
		currentCost = matrix[rowIndex][columnIndex];

		// Check if cIndex is the first base of a read
		if (std::find(lastBaseIndices.begin(), lastBaseIndices.end(), cIndex) != lastBaseIndices.end()) {
			isLastBase = true;
		} else {
			isLastBase = false;
		} 

		// Set the costs of the different operations, 
		// ensuring we don't go out of bounds of the matrix.
		if (rowIndex > 0 && columnIndex > 0) {
			if (isLastBase) {
				deletion = matrix[rowIndex][columnIndex-1]; 
			} else {
				deletion = matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-');
			}
			insert = matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]);
			substitute = matrix[rowIndex-1][columnIndex-1] + cost(ref[urIndex], clr[cIndex]);	
		} else if (rowIndex <= 0 && columnIndex > 0) {
			if (isLastBase) {
				deletion = matrix[rowIndex][columnIndex-1];
			} else {
				deletion = matrix[rowIndex][columnIndex-1] + cost(ref[urIndex], '-');
			}
			insert = infinity;
			substitute = infinity;
		} else if (rowIndex > 0 && columnIndex <= 0) {
			deletion = infinity;
			insert = matrix[rowIndex-1][columnIndex] + cost('-', clr[cIndex]);
			substitute = infinity;
		} 

		if (rowIndex == 0 || columnIndex == 0) {
				//std::cout << "Path 6\n";
				if (rowIndex == 0) {
					// Mark the beginning of a trimmed long read
					if (firstDeletion) { 
						clrMaf = 'X' + clrMaf; 
						ulrMaf = '-' + ulrMaf;
						refMaf = '-' + refMaf;
					}
					firstDeletion = false;
					clrMaf = '-' + clrMaf; 
					ulrMaf = ulr[urIndex] + ulrMaf;
					refMaf = ref[urIndex] + refMaf;
					columnIndex--;
				} else {
					//std::cout << "Insertion\n";
					clrMaf = clr[cIndex] + clrMaf;
					ulrMaf = '-' + ulrMaf;
					refMaf = '-' + refMaf;
					rowIndex--;
				}
		} else if (deletion == currentCost) {
			//std::cout << "Deletion\n";
			// Marke the beginning of a trimmed long read
			if (isLastBase and firstDeletion) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}
			clrMaf = '-' + clrMaf;
			ulrMaf = ulr[urIndex] + ulrMaf;
			refMaf = ref[urIndex] + refMaf;

			// If we're at the first base of both sequences and we are in a corrected
			// segment, add an X
			if (cIndex == 0 and urIndex == 0) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}

			columnIndex--;
			firstDeletion = false;
		} else if (insert == currentCost) {
			//std::cout << "Insertion\n";
			// Mark the end of a trimmed long read
			if (isLastBase) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}	
			clrMaf = clr[cIndex] + clrMaf;
			ulrMaf = '-' + ulrMaf;
			refMaf = '-' + refMaf;

			// If we're at the first base of both sequences and we are in a corrected
			// segment, add an X
			if (cIndex == 0 and urIndex == 0) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}

			rowIndex--;
			firstDeletion = true;
		} else if (substitute == currentCost) {
			// Mark the end of a trimmed long read
			if (isLastBase) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}	
			//std::cout << "Substitution\n";
			clrMaf = clr[cIndex] + clrMaf;
			ulrMaf = ulr[urIndex] + ulrMaf;
			refMaf = ref[urIndex] + refMaf;

			// If we're at the first base of both sequences and we are in a corrected
			// segment, add an X
			if (cIndex == 0 and urIndex == 0) {
				clrMaf = 'X' + clrMaf;
				ulrMaf = '-' + ulrMaf;
				refMaf = '-' + refMaf;
			}

			rowIndex--;
			columnIndex--;
			firstDeletion = true;
		} else {
			std::cout << "ERROR: No paths found. Terminating backtracking.\n";
			std::cout << "cIndex is " << cIndex << "\n";
			std::cout << "urIndex is " << urIndex << "\n";
			rowIndex = 0;
			columnIndex = 0;
		}
	}
	clr = clrMaf;
	ulr = ulrMaf;
	ref = refMaf;
}
