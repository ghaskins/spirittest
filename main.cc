
#include <iostream>
#include <list>
#include <stack>
#include <vector>

#include <boost/spirit/include/support_utree.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace Ast
{
    struct Term
    {
	enum Qualifier {
	    Q_NONE,
	    Q_TYPE,
	    Q_NAME,
	    Q_ID
	};

	enum Operation {
	    OP_REGEX,
	    OP_EQ,
	    OP_GT,
	    OP_LT
	};

	Term() : m_qual(Q_NONE), m_op(OP_REGEX) {}

	Qualifier   m_qual;
	Operation   m_op;
	std::string m_val;
    };

    std::ostream& operator<<(std::ostream &os, const Ast::Term &t)
    {
	os << t.m_qual << ":" << t.m_val;
	
	return os;
    }
};

BOOST_FUSION_ADAPT_STRUCT(
    Ast::Term,
    (Ast::Term::Qualifier, m_qual)
    (Ast::Term::Operation, m_op)
    (std::string, m_val)
    )

struct qualifier_ : qi::symbols<char, unsigned>
{
    qualifier_()
    {
        add
	    ("type", Ast::Term::Q_TYPE)
	    ("name", Ast::Term::Q_NAME)
	    ("id", Ast::Term::Q_ID)
        ;
    }

} qualifier;

struct operation_ : qi::symbols<char, unsigned>
{
    operation_()
    {
        add
	    (":", Ast::Term::OP_REGEX)
	    ("=", Ast::Term::OP_EQ)
	    (">", Ast::Term::OP_GT)
	    ("<", Ast::Term::OP_LT)
        ;
    }

} operation;

template <typename Iterator>
struct query_grammar
    : qi::grammar<Iterator, Ast::Program(), ascii::space_type>
{
    query_grammar()
	: query_grammar::base_type(term)
    {
	using qi::lit;
	using qi::lexeme;
	using qi::eol;
	using qi::_a;
	using qi::_1;
	using boost::phoenix::push_back;
	using ascii::char_;
	using ascii::string;
	using namespace qi::labels;
    
	term = lexeme[
	    !lit('(') >>
	    -(qualifier >> operation) >>
	    +(
		char_
		- (
		    lit(' ') |
		    lit(')') |
		    lit('&') |
		    lit('|') |
		    eol
		    )
		)
	    ];
    }
  
    qi::rule<Iterator, Ast::Term(), ascii::space_type> term;
};


int main(int argc, char **argv) {
    int ret;
    std::string query;

    po::options_description desc("Allowed options");
    desc.add_options()
	("help,h", "produces help message")
	("query", po::value<std::string>(&query),
	 "query to execute")
	;

    po::positional_options_description p;
    p.add("query", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help") || !vm.count("query")) {
	std::cerr << desc << std::endl;
	return -1;
    }

    using boost::spirit::ascii::space;
    typedef std::string::iterator iterator_type;
    typedef query_grammar<iterator_type> query_grammar;

    query_grammar g; // Our grammar

    Ast::Term result;

    bool r = phrase_parse(query.begin(), query.end(), g, space, result);

    if (!r) {
	std::cout << "Error: \"" << query << "\" is not a valid query" << std::endl;
	return -1;
    }

    std::cout << "query: "       << query
	      << " r: "          << r
	      << " results: "    << result
	      << std::endl;

}
