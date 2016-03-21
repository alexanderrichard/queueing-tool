/*
 * Parser.cc
 *
 *  Created on: Feb 17, 2016
 *      Author: richard
 */

#include "Parser.hh"
#include <fstream>
#include <algorithm>
#include <map>
#include <sstream>
#include <stdio.h>
#include "Lock.hh"
#include "Utils.hh"

Parser::Parser(const std::string& script) {
	std::vector<std::string> v;
	std::vector<std::string> vv;
	Utils::tokenizeString(v, script, " "); // remove parameters that may be passed to the script
	Utils::tokenizeString(vv, v.front(), "/");
	for (u32 i = 0; i < vv.size() - 1; i++) {
		scriptDirectory_.append(vv[i]);
		scriptDirectory_.append("/");
	}
	script_ = vv.back();
	for (u32 i = 1; i < v.size(); i++) {
		scriptParameters_.append(v[i]);
		if (i < v.size() - 1)
			scriptParameters_.append(" ");
	}
}

void Parser::initialize() {
	std::string filename = scriptDirectory_;
	filename.append(script_);
	std::ifstream fin(filename.c_str());
	/* error handling if script can not be opened */
	if (!fin.is_open()) {
		std::cerr << "ERROR: Could not read " << filename << "." << std::endl;
		Lock::unlock();
		exit(1);
	}

	/* read all lines and determine where #sequential/#parallel subscripts start */
	std::string line;
	while (std::getline(fin, line)) {
		lines_.push_back(line);
		line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
		if (line.substr(0, 6).compare("#block") == 0) {
			// remove spaces from #block line
			lines_.back().erase(std::remove_if(lines_.back().begin(), lines_.back().end(), isspace), lines_.back().end());
			blockStarts_.push_back(lines_.size() - 1);
		}
	}

	/* error handling if no stages (i.e. no #sequential/#parallel section) could be found */
	if (blockStarts_.size() == 0) {
		std::cerr << "ERROR: " << filename << " does not contain a #block section." << std::endl;
		Lock::unlock();
		exit(1);
	}
	fin.close();
}

void Parser::parseParams(const std::string& line, BlockParameters& params) {
	// initialize parameter map with default values
	params.name = "unnamed-job";
	params.threads = 1;
	params.memory = 1024;
	params.hours = 1;
	params.gpu = false;
	params.subtasks = 1;
	// parse parameters from line
	std::vector<std::string> result;
	Utils::tokenizeString(result, line, ",");
	// loop over all specified parameters and store them in a map
	for (u32 i = 0; i < result.size(); i++) {
		std::vector<std::string> pair;
		Utils::tokenizeString(pair, result.at(i), "=");
		// make sure that the parameters have a left hand side and a right hand side (two elements)
		if (pair.size() != 2) {
			std::cerr << "ERROR: Parameter specification " << result.at(i) << " does not have a valid format." << std::endl;
			Lock::unlock();
			exit(1);
		}
		// read specified parameters
		if ((pair[0].compare("name") == 0) && (pair[1].size() > 0))
			params.name = pair[1];
		else if ((pair[0].compare("threads") == 0) && (pair[1].size() > 0))
			params.threads = std::max(1, atoi(pair[1].c_str()));
		else if ((pair[0].compare("memory") == 0) && (pair[1].size() > 0))
			params.memory = std::max(1, atoi(pair[1].c_str()));
		else if ((pair[0].compare("hours") == 0) && (pair[1].size() > 0))
			params.hours = std::max(1, atoi(pair[1].c_str()));
		else if ((pair[0].compare("gpu") == 0) && (pair[1].size() > 0))
			params.gpu = (pair[1].compare("true") == 0 ? true : false);
		else if ((pair[0].compare("subtasks") == 0) && (pair[1].size() > 0))
			params.subtasks = std::max(1, atoi(pair[1].c_str()));
		else {
			std::cerr << "ERROR: Parameter specification " << result.at(i) << " does not have a valid format." << std::endl;
			Lock::unlock();
			exit(1);
		}
	}
}

void Parser::parse(const std::string& line, BlockParameters& params) {
	std::string str;
	if (line.substr(0, 7).compare("#block(") == 0)
		str = line.substr(7); // remove "#block("
	else {
		std::cerr << "ERROR: Parameter specification " << line << " does not have a valid format." << std::endl;
		Lock::unlock();
		exit(1);
	}
	str.resize(str.size() - 1); // remove ")"
	parseParams(str, params);
}

void Parser::parse() {
	// parse blocks
	blocks_.resize(blockStarts_.size());
	for (u32 i = 0; i < blocks_.size(); i++) {
		parse(lines_.at(blockStarts_.at(i)), blocks_.at(i));
		blocks_.at(i).startLine = blockStarts_.at(i);
		blocks_.at(i).endLine = (i == blocks_.size() - 1 ? lines_.size() - 1 : blockStarts_.at(i+1) - 1);
		std::stringstream filename;
		filename << scriptDirectory_ << "." << script_ << "." << i+1;
		blocks_.at(i).script = filename.str();
	}

	// generate script for each block
	for (u32 i = 0; i < blocks_.size(); i++) {
		std::ofstream fout(blocks_.at(i).script.c_str());
		if (!fout.is_open()) {
			std::cerr << "ERROR: Could not create " << blocks_.at(i).script << "." << std::endl;
			Lock::unlock();
			exit(1);
		}
		// write content
		fout << "#!/bin/bash" << std::endl;
		fout << "N_SUBTASKS=" << blocks_.at(i).subtasks << std::endl;
		fout << "SUBTASK_ID=${@: -1}" << std::endl; // last passed parameter is the subtask id
		for (u32 j = blocks_.at(i).startLine; j <= blocks_.at(i).endLine; j++) {
			fout << lines_.at(j) << std::endl;
		}
		fout.close();
	}
}
