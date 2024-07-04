/*
    This file is part of solidity.

    solidity is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    solidity is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Unit tests for KnowledgeBase
 */

#include <test/Common.h>

#include <test/libyul/Common.h>

#include <libyul/Object.h>
#include <libyul/optimiser/KnowledgeBase.h>
#include <libyul/optimiser/SSAValueTracker.h>
#include <libyul/optimiser/CommonSubexpressionEliminator.h>
#include <libyul/backends/evm/EVMDialect.h>

#include <liblangutil/ErrorReporter.h>

#include <boost/test/unit_test.hpp>

using namespace solidity::langutil;

namespace solidity::yul::test
{

class KnowledgeBaseTest
{
protected:
	KnowledgeBase constructKnowledgeBase(std::string const& _source)
	{
		ErrorList errorList;
		std::shared_ptr<AsmAnalysisInfo> analysisInfo;
		std::tie(m_object, analysisInfo) = yul::test::parse(_source, m_dialect, errorList);
		BOOST_REQUIRE(m_object && errorList.empty() && m_object->code);

		std::set<YulName> reserved{};
		OptimiserStepContext context{m_object->code->nameRepository().dialect(), m_object->code->nameRepository(), reserved, 0};
		CommonSubexpressionEliminator::run(context, m_object->code->block());

		m_ssaValues(m_object->code->block());
		for (auto const& [name, expression]: m_ssaValues.values())
			m_values[name].value = expression;

		return KnowledgeBase([this](YulName _var) { return util::valueOrNullptr(m_values, _var); }, m_object->code->nameRepository());
	}

	EVMDialect m_dialect{EVMVersion{}, true};
	std::shared_ptr<Object> m_object;
	SSAValueTracker m_ssaValues;
	std::map<YulName, AssignedValue> m_values;
};

BOOST_FIXTURE_TEST_SUITE(KnowledgeBase, KnowledgeBaseTest)

BOOST_AUTO_TEST_CASE(basic)
{
	yul::KnowledgeBase kb = constructKnowledgeBase(R"({
		let a := calldataload(0)
		let b := calldataload(0)
		let zero := 0
		let c := add(b, 0)
		let d := mul(b, 0)
		let e := sub(a, b)
	})");
	auto const& nameRepository = m_object->code->nameRepository();

	BOOST_CHECK(!kb.knownToBeDifferent(nameRepository.nameOfLabel("a"), nameRepository.nameOfLabel("b")));
	// This only works if the variable names are the same.
	// It assumes that SSA+CSE+Simplifier actually replaces the variables.
	BOOST_CHECK(!kb.valueIfKnownConstant(nameRepository.nameOfLabel("a")));
	BOOST_CHECK(kb.valueIfKnownConstant(nameRepository.nameOfLabel("zero")) == u256(0));
	BOOST_CHECK(kb.differenceIfKnownConstant(nameRepository.nameOfLabel("a"), nameRepository.nameOfLabel("b")) == u256(0));
	BOOST_CHECK(kb.differenceIfKnownConstant(nameRepository.nameOfLabel("a"), nameRepository.nameOfLabel("c")) == u256(0));
	BOOST_CHECK(kb.valueIfKnownConstant(nameRepository.nameOfLabel("e")) == u256(0));
}

BOOST_AUTO_TEST_CASE(difference)
{
	yul::KnowledgeBase kb = constructKnowledgeBase(R"({
		let a := calldataload(0)
		let b := add(a, 200)
		let c := add(a, 220)
		let d := add(12, c)
		let e := sub(c, 12)
	})");
	auto const& nameRepository = m_object->code->nameRepository();

	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("c"), nameRepository.nameOfLabel("b")) ==
		u256(20)
	);
	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("b"), nameRepository.nameOfLabel("c")) ==
		u256(-20)
	);
	BOOST_CHECK(!kb.knownToBeDifferentByAtLeast32(nameRepository.nameOfLabel("b"), nameRepository.nameOfLabel("c")));
	BOOST_CHECK(kb.knownToBeDifferentByAtLeast32(nameRepository.nameOfLabel("b"), nameRepository.nameOfLabel("d")));
	BOOST_CHECK(kb.knownToBeDifferentByAtLeast32(nameRepository.nameOfLabel("a"), nameRepository.nameOfLabel("b")));
	BOOST_CHECK(kb.knownToBeDifferentByAtLeast32(nameRepository.nameOfLabel("b"), nameRepository.nameOfLabel("a")));

	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("e"), nameRepository.nameOfLabel("a")) == u256(208)
	);
	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("e"), nameRepository.nameOfLabel("b")) == u256(8)
	);
	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("a"), nameRepository.nameOfLabel("e")) == u256(-208)
	);
	BOOST_CHECK(
		kb.differenceIfKnownConstant(nameRepository.nameOfLabel("b"), nameRepository.nameOfLabel("e")) == u256(-8)
	);
}


BOOST_AUTO_TEST_SUITE_END()

}
