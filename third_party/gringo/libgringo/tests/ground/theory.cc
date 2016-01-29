// {{{ GPL License

// This file is part of gringo - a grounder for logic programs.
// Copyright (C) 2013  Roland Kaminski

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// }}}

#include "tests/tests.hh"
#include "tests/ground/grounder_helper.hh"

namespace Gringo { namespace Ground { namespace Test {

// {{{ declaration of TestTheory

class TestTheory : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(TestTheory);
        CPPUNIT_TEST(test_directive);
        CPPUNIT_TEST(test_head);
        CPPUNIT_TEST(test_body);
        CPPUNIT_TEST(test_term);
        CPPUNIT_TEST(test_guard);
    CPPUNIT_TEST_SUITE_END();
    using S = std::string;

public:
    void test_directive();
    void test_head();
    void test_body();
    void test_term();
    void test_guard();
    virtual ~TestTheory();
};

// }}}

// {{{ definition of TestTheory

void TestTheory::test_directive() {
    std::string theory =
        "#theory t {"
        "    group { };"
        "    &a/0 : group, directive"
        "}.";
    CPPUNIT_ASSERT_EQUAL(
        S("&a{}.\n"),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { }."));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{1; 2; 3; f(1); f(2); f(3)}.\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1..3)."
            "&a { X : p(X); f(X) : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{1: p(1); 2: p(2); 3: p(3); f(1): p(1); f(2): p(2); f(3): p(3)}.\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1..3)}."
            "&a { X : p(X); f(X) : p(X) }."
        ));
}

void TestTheory::test_head() {
    std::string theory =
        "#theory t {"
        "    group { };"
        "    &a/0 : group, head"
        "}.";
    CPPUNIT_ASSERT_EQUAL(
        S("&a{}.\n"),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { }."));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{1; 2; 3; f(1); f(2); f(3)}.\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1..3)."
            "&a { X : p(X); f(X) : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{1: p(1); 2: p(2); 3: p(3); f(1): p(1); f(2): p(2); f(3): p(3)}.\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1..3)}."
            "&a { X : p(X); f(X) : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{1: p(1); 2: p(2); 3: p(3)}:-p(1).\n"
            "&a{1: p(1); 2: p(2); 3: p(3)}:-p(2).\n"
            "&a{1: p(1); 2: p(2); 3: p(3)}:-p(3).\n"
            "p(2):-p(1).\n"
            "p(3):-p(2).\n"
            "{p(1)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1)}."
            "p(X+1) :- p(X), X < 3."
            "&a { Y : p(Y) } :- p(X)."
        ));
}

void TestTheory::test_body() {
    std::string theory =
        "#theory t {"
        "    group { };"
        "    &a/0 : group, body"
        "}.";
    CPPUNIT_ASSERT_EQUAL(
        S(":-&a{}.\n"),
        Gringo::Ground::Test::groundText(
            theory +
            ":-&a { }."));
    CPPUNIT_ASSERT_EQUAL(
        S(
            ":-&a{1; 2; 3; f(1); f(2); f(3)}.\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1..3)."
            ":-&a { X : p(X); f(X) : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            ":-&a{1: p(1); 2: p(2); 3: p(3); f(1): p(1); f(2): p(2); f(3): p(3)}.\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1..3)}."
            ":-&a { X : p(X); f(X) : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            ":-not &a{1: p(1); 2: p(2); 3: p(3)}.\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1..3)}."
            ":-not &a { X : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            ":-not not &a{1: p(1); 2: p(2); 3: p(3)}.\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "{p(1..3)}."
            ":-not not &a { X : p(X) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "p(1).\n"
            "p(2):-&a{1; 1; 2: p(2); 2: p(2); 3: p(3)}.\n"
            "p(3):-&a{1; 1; 2: p(2); 2: p(2); 3: p(3)},p(2).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1)."
            "p(X+1) :- p(X), X < 3, &a { Y : p(Y) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "p(1).\n"
            "p(2):-not &a{1; 2: p(2); 3: p(3)}.\n"
            "p(3):-not &a{1; 2: p(2); 3: p(3)},p(2).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1)."
            "p(X+1) :- p(X), X < 3, not &a { Y : p(Y) }."
        ));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "p(1).\n"
            "p(2):-not not &a{1; 2: p(2); 3: p(3)}.\n"
            "p(3):-not not &a{1; 2: p(2); 3: p(3)},p(2).\n"
            ),
        Gringo::Ground::Test::groundText(theory +
            "p(1)."
            "p(X+1) :- p(X), X < 3, not not &a { Y : p(Y) }."
        ));
}

void TestTheory::test_guard() {
    std::string theory =
        "#theory t {\n"
        "    group { };\n"
        "    &a/0 : group, {=,>=}, group, head\n"
        "}.\n";
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{}=(1).\n"
            "&a{}=(2).\n"
            "&a{}=(3).\n"),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { } = X :- X=1..3."));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{}>=(1).\n"
            "&a{}>=(2).\n"
            "&a{}>=(3).\n"),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { } >= X :- X=1..3."));
}

void TestTheory::test_term() {
    std::string theory =
        "#theory t {\n"
        "  group {\n"
        "    + : 4, unary;\n"
        "    - : 4, unary;\n"
        "    ^ : 3, binary, right;\n"
        "    * : 2, binary, left;\n"
        "    + : 1, binary, left;\n"
        "    - : 1, binary, left\n"
        "  };\n"
        "  &a/0 : group, head\n"
        "}.\n";
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{: }.\n"
        ),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { : }."));
    CPPUNIT_ASSERT_EQUAL(
        S(
            "&a{#inf}.\n"
            "&a{#sup}.\n"
            "&a{(-2)}.\n"
            "&a{(-c)}.\n"
            "&a{(-f(c))}.\n"
            "&a{(1,2)}.\n"
            "&a{1}.\n"
            "&a{c}.\n"
            "&a{f(c)}.\n"
        ),
        Gringo::Ground::Test::groundText(
            theory +
            "&a { X } :- X=(1;c;f(c);-2;-f(c);-c;#sup;#inf;(1,2))."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{()}.\n"), Gringo::Ground::Test::groundText(theory + "&a { () }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(1,)}.\n"), Gringo::Ground::Test::groundText(theory + "&a { (1,) }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(1,2)}.\n"), Gringo::Ground::Test::groundText(theory + "&a { (1,2) }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{[1,2]}.\n"), Gringo::Ground::Test::groundText(theory + "&a { [1,2] }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{{1,2}}.\n"), Gringo::Ground::Test::groundText(theory + "&a { {1,2} }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(-1)}.\n"), Gringo::Ground::Test::groundText(theory + "&a { (-1) }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(-(+1))}.\n"), Gringo::Ground::Test::groundText(theory + "&a { - + 1 }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(1^(2^3))}.\n"), Gringo::Ground::Test::groundText(theory + "&a { 1^2^3 }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{((1*2)*3)}.\n"), Gringo::Ground::Test::groundText(theory + "&a { 1*2*3 }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(1+(2*(3^4)))}.\n"), Gringo::Ground::Test::groundText(theory + "&a { 1+2*3^4 }."));
    CPPUNIT_ASSERT_EQUAL(S( "&a{(1+((-2)*((-3)^4)))}.\n"), Gringo::Ground::Test::groundText(theory + "&a { 1 + -2 * -3^4 }."));
}

TestTheory::~TestTheory() { }

// }}}

CPPUNIT_TEST_SUITE_REGISTRATION(TestTheory);

} } } // namespace Test Ground Gringo

