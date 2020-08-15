/***************************************************************************
 *  pddl_grammar.h - Definition of PDDL grammar as BOOST spirit
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

#ifndef PDDLQI_PARSER_PDDLGRAMMAR_H
#define PDDLQI_PARSER_PDDLGRAMMAR_H

#include "pddl_ast.h"

#include <boost/spirit/home/support/info.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_real.hpp>
#include <boost/spirit/repository/include/qi_distinct.hpp>
#include <exception>
#include <iostream>
#include <sstream>

namespace pddl_parser {

/**
 * @brief Exception class used for parsing failures
 *
 */
class ParserException : std::exception
{
public:
	/**
	 * @brief Construct a new Parser Exception object without any available information
	 *
	 */
	ParserException() : exception()
	{
		message = "Unknown ParserException";
	}

	/**
	 * @brief Construct a new Parser Exception object with a given Iterator type for denoting the
	 *        position of an unknown error
	 *
	 * @tparam Iterator Iteratortype
	 * @param start Starting point of the object to be parsed
	 * @param current Current position of the parsing
	 * @param end End point of the object to be parsed
	 */
	template <typename Iterator>
	ParserException(Iterator start, Iterator current, Iterator end) : exception()
	{
		message = "Unknown error occured here: ";
		message += std::string(current, end);
	}

	/**
	 * @brief Construct a new Parser Exception object for an error where something is a expected but not found
	 *
	 * @tparam Iterator Iteratortype
	 * @param expectedRule Expected rule at the current position
	 * @param start Starting point of the object to be parsed
	 * @param end End position of the object to be parsed
	 * @param current Current position of the parsing
	 */
	template <typename Iterator>
	ParserException(const boost::spirit::info &expectedRule,
	                Iterator                   start,
	                Iterator                   end,
	                Iterator                   current)
	: exception()
	{
		std::stringstream messageStream;
		messageStream << "Parse Error: Expected: ";
		messageStream << expectedRule;
		messageStream << " here: ";
		messageStream << std::string(current, end);

		message = messageStream.str();
	}

	virtual ~ParserException() throw()
	{
	}

	/**
	 * @brief Returns the current exception message
	 *
	 * @return const char* Current exception message
	 */
	virtual const char *
	what() const throw()
	{
		return message.c_str();
	}

protected:
	/**
	 * @brief String storing information about the exception
	 *
	 */
	std::string message;
};

namespace Grammar {
namespace fusion  = boost::fusion;
namespace phoenix = boost::phoenix;
namespace qi      = boost::spirit::qi;
namespace ascii   = boost::spirit::ascii;

using ascii::char_;
using boost::spirit::repository::distinct;
using qi::fail;
using qi::float_;
using qi::lazy;
using qi::lexeme;
using qi::lit;
using qi::on_error;
using namespace qi::labels;

using phoenix::at_c;
using phoenix::begin;
using phoenix::bind;
using phoenix::clear;
using phoenix::construct;
using phoenix::end;
using phoenix::if_else;
using phoenix::insert;
using phoenix::push_back;
using phoenix::ref;
using phoenix::val;

/** @class pddl_skipper
        * A skipper for PDDL files.
        * This skipper skips spaces and comments starting with ';'
        */
template <typename Iterator>
struct pddl_skipper : public qi::grammar<Iterator>
{
	pddl_skipper() : pddl_skipper::base_type(skip, "PDDL")
	{
		skip = ascii::space | (';' >> *(qi::char_ - qi::eol));
	}
	/** The actual skipping rule. */
	qi::rule<Iterator> skip;
};

/**
 * @brief boost::spirit::qi::symbols struct defining all possible requirements symbols
 *
 */
struct RequirementFlagSymbols_ : qi::symbols<char, pddl_parser::RequirementFlag::EnumType>
{
	RequirementFlagSymbols_()
	{
		add(":strips", RequirementFlag::eStrips)(":negative-preconditions",
		                                         RequirementFlag::eNegativePreconditions)(
		  ":typing", RequirementFlag::typing)(":action-costs",
		                                      RequirementFlag::action_cost)(":adl",
		                                                                    RequirementFlag::adl);
	}
};

/**
 * @brief boost::spirit::qi::symbols struct defining operator symbols
 *
 */
struct OperatorSymbols_ : qi::symbols<char, pddl_parser::OperatorFlag::EnumType>
{
	OperatorSymbols_()
	{
		this->add("and", OperatorFlag::conjunction)("not", OperatorFlag::negation)(
		  "or", OperatorFlag::disjunction)("when", OperatorFlag::condition);
	}
};

void insert_typed_name_entities(TypedList &                     entities,
                                const std::vector<std::string> &names,
                                const std::string &             type);

/**
 * @brief Struct defining the grammar for PDDL Domains.
 *
 * @tparam Iterator to the string to be parsed
 * @tparam pddl_skipper<Iterator> Skipper defining what parts of the input to be skipped (e.g. comments, spaces)
 */
template <typename Iterator, typename Skipper = pddl_skipper<Iterator>>
struct Domain : qi::grammar<Iterator, PddlDomain(), Skipper>
{
	Domain() : Domain::base_type(pddlDomain, "PDDL Domain")
	{
		name %= lexeme[char_("a-zA-Z") >> *(char_("a-zA-Z0-9_-"))];
		name.name("name");

		variable %= lit('?') > name;
		variable.name("variable");

		type %= name;
		type.name("type");

		typedListExplicitType = (+(lazy(_r1)[push_back(_a, _1)])) > lit('-')
		                        > type[bind(&insert_typed_name_entities, _val, _a, _1)];
		typedListExplicitType.name("typedListExplicitType");
		char obj[] = "object";
		typedList  = (*(typedListExplicitType(_r1)[insert(_val, end(_val), begin(_1), end(_1))]))
		            > (*(lazy(_r1)[push_back(_val, construct<struct Entity>(_1, &obj[0]))]));
		typedList.name("typedList");

		term = name[at_c<0>(_val) = false, at_c<1>(_val) = _1]
		       | variable[at_c<0>(_val) = true, at_c<1>(_val) = _1];
		term.name("term");

		atomicFormula = name[at_c<0>(_val) = _1] > (*term)[at_c<1>(_val) = _1];
		atomicFormula.name("atomicFormula");

		literal = atomicFormula[at_c<0>(_val) = false, at_c<1>(_val) = _1]
		          | (lit("not") > lit('(') > atomicFormula[at_c<0>(_val) = true, at_c<1>(_val) = _1]
		             > lit(')'));
		literal.name("literal");

		goalDescription %= lit('(') >> (functionalCondition[_val = _1] | atomicFormula[_val = _1])
		                   >> qi::eps >> lit(')');
		goalDescription.name("goalDescription");

		conditionalEffect = goalDescription >> effect;
		conditionalEffect.name("conditionalEffect");

		bool condition_flag;
		functionalEffect =
		  operatorSymbols[at_c<0>(_val)                = _1,
		                  phoenix::ref(condition_flag) = qi::_1 == OperatorFlag::condition]
		  >> !(char_("a-zA-Z0-9_"))
		  >> ((qi::eps(phoenix::ref(condition_flag) == true) >> conditionalEffect)
		      | (qi::eps(phoenix::ref(condition_flag) == false) >> +(effect)))[at_c<1>(_val) = _1];
		functionalEffect.name("functionalEffect");

		actionCost =
		  distinct(char_("a-zA-Z_0-9"))["increase"] > lit('(') >> name > lit(')') >> qi::int_;

		effect = lit('(') >> (functionalEffect | actionCost | atomicFormula) > lit(')');
		effect.name("effect");

		functionalCondition = operatorSymbols >> !(char_("a-zA-Z0-9_")) >> +(goalDescription);
		functionalCondition.name("functionalCondition");

		requireDef %= -(lit('(') >> lit(":requirements") > (+(requirementFlagSymbols)) > lit(')'));
		requireDef.name("requireDef");

		typesDef %= -(lit('(') >> lit(":types") > typedList(phoenix::ref(name)) > lit(')'));
		typesDef.name("typesDef");

		constantsDef %= -(lit('(') >> lit(":constants") > typedList(phoenix::ref(name)) > lit(')'));
		constantsDef.name("constantsDef");

		predicatesDef =
		  -(lit('(') >> lit(":predicates")
		    > (+(lit('(') > name[_a = _1] >> typedList(phoenix::ref(variable))[_b = _1]
		         > lit(')'))[push_back(_val, construct<std::pair<std::string, TypedList>>(_a, _b))])
		    > lit(')'));
		predicatesDef.name("predicatesDef");

		pddlAction = lit('(') > (lit(":action") | lit("durative-action")) > name[at_c<0>(_val) = _1]
		             > lit(":parameters") > lit("(")
		             > typedList(phoenix::ref(variable))[at_c<1>(_val) = _1] > lit(")")
		             > lit(":precondition")
		             >> (goalDescription[at_c<2>(_val) = _1] | (lit('(') > lit(')')))
		             > (lit(":effect") >> (effect[at_c<3>(_val) = _1] | (lit('(') > lit(')'))))
		             > -(lit(":duration") >> float_) > -(lit(":cond-breakup") >> goalDescription)
		             > -(lit(":temp-breakup") >> goalDescription) > lit(')');
		pddlAction.name("pddlAction");

		actionsDef = *pddlAction;
		actionsDef.name("actionsDef");

		pddlDomain = lit('(') > lit("define") > lit('(') > lit("domain") > name[at_c<0>(_val) = _1]
		             > lit(')') > requireDef[at_c<1>(_val) = _1] > typesDef[at_c<2>(_val) = _1]
		             > constantsDef[at_c<3>(_val) = _1] > predicatesDef[at_c<4>(_val) = _1]
		             > actionsDef[at_c<5>(_val) = _1] > lit(')');
		pddlDomain.name("pddlDomain");

		on_error<fail>(pddlDomain, construct<ParserException>(qi::_4, qi::_1, qi::_2, qi::_3));
	}

	/**
	 * @brief Struct containing all needed requirements for the PDDL Domain
	 */
	struct RequirementFlagSymbols_ requirementFlagSymbols;

	/**
	 * @brief Simple rule, matching a string
	 */
	qi::rule<Iterator, std::string(), Skipper> StringRule;
	/**
	 * @brief Rule, matching a complete PDDL Domain
	 */
	qi::rule<Iterator, PddlDomain(), Skipper> pddlDomain;
	/**
	 * @brief Rule, matching the requiremenst list of a PDDL Domain
	 */
	qi::rule<Iterator, RequirementFlag::VectorType(), Skipper> requireDef;
	/**
	 * @brief Rule, matching the type definition list of a PDDL Domain
	 */
	qi::rule<Iterator, TypedList(), Skipper> typesDef;
	/**
	 * @brief Rule, matching the constants definition list of a PDDL Domain
	 */
	qi::rule<Iterator, TypedList(), Skipper> constantsDef;
	/**
	 * @brief Rule, matching a single PDDL Action
	 */
	qi::rule<Iterator, PddlAction(), Skipper> pddlAction;
	/**
	 * @brief Rule, matching the list of defined actions in a PDDL Domain
	 */
	qi::rule<Iterator, ActionList(), Skipper> actionsDef;
	/**
	 * @brief Rule, matching a list of predicates
	 */
	qi::rule<Iterator, PredicateList(), qi::locals<std::string, TypedList>, Skipper> predicatesDef;
	/**
	 * @brief Rule, matching a conditional effect
	 */
	qi::rule<Iterator, ConditionalEffect(), Skipper> conditionalEffect;
	/**
	 * @brief Rule, matching a functional condition
	 */
	qi::rule<Iterator, FunctionalCondition(), Skipper> functionalCondition;
	/**
	 * @brief Rule, matching a functional effect
	 */
	qi::rule<Iterator, FunctionalEffect(), Skipper> functionalEffect;
	/**
	 * @brief Rule, matching an action cost
	 */
	qi::rule<Iterator, ActionCost(), Skipper> actionCost;
	/**
	 * @brief Rule, matching a effect
	 */
	qi::rule<Iterator, Effect(), Skipper> effect;
	/**
	 * @brief Rule, matching a goal description
	 */
	qi::rule<Iterator, GoalDescription(), Skipper> goalDescription;
	/**
	 * @brief Rule, matching a literal
	 */
	qi::rule<Iterator, Literal(), Skipper> literal;
	/**
	 * @brief Rule, matching an atomic formula
	 */
	qi::rule<Iterator, AtomicFormula(), Skipper> atomicFormula;
	/**
	 * @brief Rule, matching a term
	 */
	qi::rule<Iterator, Term(), Skipper> term;
	/**
	 * @brief Rule, matching a typed list
	 */
	qi::rule<Iterator, TypedList(StringRule), Skipper> typedList;
	/**
	 * @brief Rule, matching a explicit typed List
	 */
	qi::rule<Iterator, TypedList(StringRule), qi::locals<std::vector<std::string>>, Skipper>
	  typedListExplicitType;
	/**
	 * @brief Rule, matching a type, which is a string
	 */
	StringRule type;
	/**
	 * @brief Rule, matching a name, which is a string
	 */
	StringRule name;
	/**
	 * @brief Rule, matching a variable, which is a string
	 */
	StringRule variable;

	/**
	 * @brief struct containing all possible logical operator symbols
	 */
	struct OperatorSymbols_ operatorSymbols;
};

/**
 * @brief Struct defining the grammar for a PDDL Problem description
 *
 * @tparam Iterator Iterator for the input string
 * @tparam pddl_skipper<Iterator> A skipper defining what parts of the inputs to be ignored (e.g. comments, spaces)
 */
template <typename Iterator, typename Skipper = pddl_skipper<Iterator>>
struct Problem : qi::grammar<Iterator, PddlProblem(), Skipper>
{
	Problem() : Problem::base_type(pddlProblem, "PDDL Domain")
	{
		name %= lexeme[char_("a-zA-Z") >> *(char_("a-zA-Z0-9_-"))];
		name.name("name");

		variable %= lit('?') > name;
		variable.name("variable");

		type %= name;
		type.name("type");

		typedListExplicitType = (+(lazy(_r1)[push_back(_a, _1)])) > lit('-')
		                        > type[bind(&insert_typed_name_entities, _val, _a, _1)];
		typedListExplicitType.name("typedListExplicitType");
		char obj[] = "object";

		typedList = (*(typedListExplicitType(_r1)[insert(_val, end(_val), begin(_1), end(_1))]))
		            > (*(lazy(_r1)[push_back(_val, construct<struct Entity>(_1, &obj[0]))]));
		typedList.name("typedList");

		term = name[at_c<0>(_val) = false, at_c<1>(_val) = _1]
		       | variable[at_c<0>(_val) = true, at_c<1>(_val) = _1];
		term.name("term");

		atomicFormula = name[at_c<0>(_val) = _1] > (*term)[at_c<1>(_val) = _1];
		atomicFormula.name("atomicFormula");

		literal = atomicFormula[at_c<0>(_val) = false, at_c<1>(_val) = _1]
		          | (lit("not") > lit('(') > atomicFormula[at_c<0>(_val) = true, at_c<1>(_val) = _1]
		             > lit(')'));
		literal.name("literal");

		goalDescription %= lit('(') >> (functionalCondition[_val = _1] | atomicFormula[_val = _1])
		                   >> qi::eps >> lit(')');
		goalDescription.name("goalDescription");

		functionalCondition = operatorSymbols >> !(char_("a-zA-Z0-9_")) >> +(goalDescription);
		functionalCondition.name("functionalCondition");

		goalDef = lit('(') >> lit(":goal") >> goalDescription > lit(')');
		goalDef.name("goalDef");

		objectsDef %= -(lit('(') >> lit(":objects") > typedList(phoenix::ref(name)) > lit(')'));
		objectsDef.name("objectsDef");

		initialFacts =
		  (lit('(') >> lit(":init") > (+(lit('(') >> atomicFormula > lit(')'))) > lit(')'));
		initialFacts.name("initialFacts");

		pddlProblem = lit('(') > lit("define") > lit('(') > lit("problem") > name[at_c<0>(_val) = _1]
		              > lit(')') > lit('(') > lit("(:domain") > name[at_c<1>(_val) = _1]
		              > objectsDef[at_c<2>(_val) = _1] > initialFacts[at_c<3>(_val) = _1]
		              > goalDef[at_c<4>(_val) = _1] > lit(')');
		pddlProblem.name("pddlProblem");

		on_error<fail>(pddlProblem, construct<ParserException>(qi::_4, qi::_1, qi::_2, qi::_3));
	}

	/**
	 * @brief Simple rule, matching a string
	 */
	qi::rule<Iterator, std::string(), Skipper> StringRule;
	/**
	 * @brief Rule, matching a PDDL Problem
	 */
	qi::rule<Iterator, PddlProblem(), Skipper> pddlProblem;
	/**
	 * @brief Rule, matching the list of existing objects in a PDDL Problem
	 */
	qi::rule<Iterator, TypedList(), Skipper> objectsDef;
	/**
	 * @brief Rule, matching a list of predicates
	 */
	qi::rule<Iterator, PredicateList(), qi::locals<std::string, TypedList>, Skipper> predicatesDef;
	/**
	 * @brief Rule, matching a list of facts
	 */
	qi::rule<Iterator, FactList(), Skipper> initialFacts;
	/**
	 * @brief Simple rule, matching a functional Condition
	 */
	qi::rule<Iterator, FunctionalCondition(), Skipper> functionalCondition;
	/**
	 * @brief Simple rule, matching a goal description
	 */
	qi::rule<Iterator, GoalDescription(), Skipper> goalDescription;
	/**
	 * @brief Simple rule, matching a goal defintion
	 */
	qi::rule<Iterator, GoalDescription(), Skipper> goalDef;
	/**
	 * @brief Simple rule, matching a literal
	 */
	qi::rule<Iterator, Literal(), Skipper> literal;
	/**
	 * @brief Simple rule, matching an atomic formula
	 */
	qi::rule<Iterator, AtomicFormula(), Skipper> atomicFormula;
	/**
	 * @brief Simple rule, matching a term
	 */
	qi::rule<Iterator, Term(), Skipper> term;
	/**
	 * @brief Simple rule, matching a typed list
	 */
	qi::rule<Iterator, TypedList(StringRule), Skipper> typedList;
	/**
	 * @brief Rule, matching a list of a certain type
	 *
	 */
	qi::rule<Iterator, TypedList(StringRule), qi::locals<std::vector<std::string>>, Skipper>
	  typedListExplicitType;
	/**
	 * @brief Rule matching type definitions, which is a string
	 */
	StringRule type;
	/**
	 * @brief Rule matching name definitions, which is a string
	 */
	StringRule name;
	/**
	 * @brief Rule matching variable definitions, which is a string
	 */
	StringRule variable;

	/**
	 * @brief Struct that contains all available logical operator symbols
	 */
	struct OperatorSymbols_ operatorSymbols;
};
} // namespace Grammar
} // namespace pddl_parser

#endif
