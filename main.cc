
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
#include <boost/tokenizer.hpp>

namespace po = boost::program_options;
namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

typedef std::set<unsigned> IntSet;

template <typename Iterator>
struct query_grammar
  : qi::grammar<Iterator, IntSet, ascii::space_type>
{
  query_grammar()
    : query_grammar::base_type(expression)
  {
    using qi::lit;
    using qi::lexeme;
    using ascii::char_;
    using ascii::string;
    using namespace qi::labels;
    
    group       = '(' >> expression >> ')';
    factor      = *char_ | group;
    expression  = factor >> *(('&' >> factor) | ('|' >> factor));
  }
  
  qi::rule<Iterator, IntSet, ascii::space_type> expression;
  qi::rule<Iterator, std::string(), ascii::space_type> factor;
  qi::rule<Iterator, std::string(), ascii::space_type> group;
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

  typedef std::list<std::string> StringList;

  StringList _init = {
    "The quick brown fox jumped over the lazy dog",
    "Four score and seven years ago",
    "I have a dream"
  };

  std::vector<StringList> content;

  for(StringList::iterator iter(_init.begin()); iter != _init.end(); ++iter) {
    boost::tokenizer<> tok(*iter);
    StringList tokens;
    for(boost::tokenizer<>::iterator iter(tok.begin()); iter != tok.end(); ++iter) {
      tokens.push_back(*iter);
    }

    content.push_back(tokens);
  }

  using boost::spirit::ascii::space;
  typedef std::string::iterator iterator_type;
  typedef query_grammar<iterator_type> query_grammar;

  query_grammar g; // Our grammar

  IntSet result;

  bool r = phrase_parse(query.begin(), query.end(), g, space, result);

  std::cout << "query: " << query
	    << " result: " << r
	    << std::endl;
}
