
#include <iostream>
#include <list>
#include <vector>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

typedef std::set<unsigned> IntSet;

void do_term(const std::vector<char> &term)
{
  std::cout << "term: ";

  for (std::vector<char>::const_iterator iter(term.begin());
       iter != term.end(); ++iter)
    {
      std::cout << *iter;
    }

  std::cout << std::endl;
}

class State
{
public:
  typedef std::list<std::string> Strings;

  State() {}
  State(const Strings &strings) : m_state(strings) {}

  void push(const std::string term)
  {
    Strings matched;

    for (Strings::const_iterator iter(m_state.begin());
	 iter != m_state.end();
	 ++iter)
      {
	const std::string &item(*iter);

	if (item.find(term) != item.npos)
	  matched.push_back(item);
      }

    m_stack.push_back(matched);
  }

private:
  typedef std::list<Strings> Stack;

  Strings m_state;
  Stack   m_stack;
};

typedef std::list<std::string> ResultType;

template <typename Iterator>
struct query_grammar
  : qi::grammar<Iterator, ResultType(), ascii::space_type>
{
  query_grammar()
    : query_grammar::base_type(expression)
  {
    using qi::lit;
    using qi::lexeme;
    using qi::eol;
    using ascii::char_;
    using ascii::string;
    using namespace qi::labels;
    
    //group       = lit('(') >> expression >> lit(')');
    term = lexeme[
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

    //factor      = group | term;
    factor      = term;
    expression  = factor >> *((lit('&') >> factor) | (lit('|') >> factor));
  }
  
  qi::rule<Iterator, ResultType(), ascii::space_type> expression;
  qi::rule<Iterator, std::string(), ascii::space_type> factor;
  qi::rule<Iterator, std::string(), ascii::space_type> term;
  //qi::rule<Iterator, std::string(), ascii::space_type> group;

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

  State::Strings _init = {
    "The quick brown fox jumped over the lazy dog",
    "Four score and seven years ago",
    "I have a dream"
  };

  using boost::spirit::ascii::space;
  typedef std::string::iterator iterator_type;
  typedef query_grammar<iterator_type> query_grammar;

  query_grammar g; // Our grammar

  ResultType result;

  bool r = phrase_parse(query.begin(), query.end(), g, space, result);

  std::cout << "query: "   << query
	    << " r: "      << r
	    << " result: ";

  for (ResultType::const_iterator iter(result.begin());
       iter != result.end(); ++iter)
    {
      std::cout << ":" << *iter;
    }

  std::cout << std::endl;
}
