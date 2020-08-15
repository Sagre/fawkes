/***************************************************************************
 *  pddl_parser.h - A parser for the PDDL language
 *
 *  Created: Sun 22 Dec 2019 12:39:10 CET 12:39
 *  Copyright  2019 Daniel Habering <daniel.habering@rwth-aachen.de>
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

/*  This is based on pddl-qi (https://github.com/naderman/pddl-qi) */

#ifndef PDDLQI_PARSER_PARSER_H
#define BOOST_SPIRIT_DEBUG
#define PDDLQI_PARSER_PARSER_H

#include "pddl_grammar.h"

#include <iostream>

namespace pddl_parser {

/**
 * @brief Class offering the ability to parse a string input into a PDDL Domain or a PDDL Problem
 */
class Parser
{
public:
	/**
	* @brief parses a string that should represent a proper pddl domain
	*
	* @param input String containing the pddl domain description
	* @return PddlDomain Parsed PDDL Domain struct
	*/
	PddlDomain
	parseDomain(const std::string &input)
	{
		return parse<Grammar::Domain<std::string::const_iterator>,
		             Grammar::pddl_skipper<std::string::const_iterator>,
		             PddlDomain>(input);
	}

	/**
	 * @brief Parses a string that should represent a proper pddl problem
	 *
	 * @param input String containing the pddl problem description
	 * @return PddlProblem Parsed PDDL Problem struct
	 */
	PddlProblem
	parseProblem(const std::string &input)
	{
		return parse<Grammar::Problem<std::string::const_iterator>,
		             Grammar::pddl_skipper<std::string::const_iterator>,
		             PddlProblem>(input);
	}

	/**
	 * @brief Function that takes care about parsing an input into a given Grammar
	 *
	 * @tparam Grammar Grammar that should be used for parsing
	 * @tparam Skipper Skipper, defining what parts of the input should not be taken into regard for parsing (comments, newlines etc)
	 * @tparam Attribute Target Struct
	 * @param input String containing the text to be parsed
	 * @return Attribute Filled Attribute with the parsed input string according to the given grammar
	 */
	template <typename Grammar, typename Skipper, typename Attribute>
	Attribute
	parse(const std::string &input)
	{
		Skipper skipper;

		Grammar grammar;

		Attribute data;

		std::string::const_iterator iter = input.begin();
		std::string::const_iterator end  = input.end();

		bool r = phrase_parse(iter, end, grammar, skipper, data);

		if (!r || iter != end) {
			throw ParserException(input.begin(), iter, end);
		}

		return data;
	}
};
} // namespace pddl_parser
#endif
