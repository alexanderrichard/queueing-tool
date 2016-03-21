/*
 * Utils.hh
 *
 *  Created on: Feb 18, 2016
 *      Author: richard
 */

#ifndef UTILS_HH_
#define UTILS_HH_

#include <string.h>
#include "Types.hh"

class Utils
{
public:
	static void tokenizeString(std::vector<std::string>& result, const std::string& str, const char* delimiter) {
	    size_t start = 0, end = 0;
	    result.clear();
	    std::string delim = std::string(delimiter);

	    while (end != std::string::npos)
	    {
	        end = str.find_first_of(delim, start);
	        if (str.substr(start, (end == std::string::npos) ? std::string::npos : end - start).size() > 0) {
	        	result.push_back(str.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
	        }
	        start = end + 1;
	    }
	}

	static void stringToIntVector(std::vector<u32>& result, const std::string& str) {
		std::vector<std::string> tmp;
		tokenizeString(tmp, str, " ");
		for (u32 i = 0; i < tmp.size(); i++) {
			result.push_back(atoi(tmp[i].c_str()));
		}
	}

	static bool isInteger(const char* str) {
		std::string s(str);
		std::string::const_iterator it = s.begin();
		while (it != s.end() && std::isdigit(*it))
			++it;
		return (!s.empty()) && (it == s.end());
	}

	static bool isRange(const char* str) {
		std::string range(str);
		std::vector<std::string> v;
		tokenizeString(v, str, "-");
		if ((v.size() == 2) && isInteger(v[0].c_str()) && isInteger(v[1].c_str()))
			return true;
		return false;
	}
};

#endif /* UTILS_HH_ */
