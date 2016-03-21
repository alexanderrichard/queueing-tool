/*
 * Parser.hh
 *
 *  Created on: Feb 17, 2016
 *      Author: richard
 */

#ifndef PARSER_HH_
#define PARSER_HH_

#include <string.h>
#include <vector>
#include "Types.hh"

class Parser
{
public:
	struct BlockParameters {
		std::string name;
		u32 threads;
		u32 memory;
		u32 hours;
		bool gpu;
		u32 subtasks; // number of subtasks being executed in parallel
		u32 startLine;
		u32 endLine;
		std::string script;
	};
private:
	std::string scriptDirectory_;
	std::string script_;
	std::string scriptParameters_;
	std::vector<std::string> lines_;
	std::vector<u32> blockStarts_;
	std::vector<BlockParameters> blocks_;

	void parseParams(const std::string& line, BlockParameters& params);
	void parse(const std::string& line, BlockParameters& params);
public:
	Parser(const std::string& script);
	virtual ~Parser() {}

	void initialize();
	void parse();
	u32 nBlocks() const { return blockStarts_.size(); }
	const BlockParameters& block(u32 blockIndex) const { return blocks_.at(blockIndex); }
	const std::string& scriptDirectory() const { return scriptDirectory_; }
	const std::string& scriptParameters() const { return scriptParameters_; }
};

#endif /* PARSER_HH_ */
