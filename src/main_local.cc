/*
 * main_local.cc
 *
 *  Created on: Feb 22, 2016
 *      Author: richard
 */

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "Types.hh"
#include "Parser.hh"

void usage(const char* programName) {
	std::cout << "usage: " << programName << " <script+parameters>" << std::endl;
	exit(1);
}

int main(int argc, const char* argv[]) {

	if (argc != 2) {
		usage(argv[0]);
	}
	std::string script(argv[1]);

	Parser parser(script);
	parser.initialize();
	parser.parse();

	for (u32 i = 0; i < parser.nBlocks(); i++) {
		for (u32 subtask = 1; subtask <= parser.block(i).subtasks; subtask++) {
			std::stringstream script;
			script << parser.block(i).script << " " << parser.scriptParameters() << " " << subtask;
			std::stringstream cmd;
			cmd << "export OMP_NUM_THREADS=" << parser.block(i).threads << "; bash " << script.str();
			system(cmd.str().c_str());
		}
		remove(parser.block(i).script.c_str());
	}

	return 0;
}
