
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

typedef std::set<unsigned> IntSet;

namespace Ast
{
  typedef std::string Term;
  struct Operation;
  struct Program;
  
  typedef boost::variant<
    Term,
    boost::recursive_wrapper< Operation >,
    boost::recursive_wrapper< Program >
    > Operand;

  struct Operation
  {
    char    m_type;
    Operand m_operand;
  };

  std::ostream& operator<<(std::ostream &os, const Ast::Operation &o)
  {
    os << o.m_operand << ", " << o.m_type;
    
    return os;
  }

  struct Program
  {
    typedef std::list<Operation> Operations;

    Operand    m_first;
    Operations m_rest;
  };

  std::ostream& operator<<(std::ostream &os, const Ast::Program &p)
  {
    os << "[" << p.m_first;
    
    for (Ast::Program::Operations::const_iterator iter(p.m_rest.begin());
	 iter != p.m_rest.end(); ++iter)
      {
	os << ", " << *iter;
      }
    
    os << "]";
    
    return os;
  }
};

BOOST_FUSION_ADAPT_STRUCT(
    Ast::Operation,
    (char,         m_type)
    (Ast::Operand, m_operand)
)

BOOST_FUSION_ADAPT_STRUCT(
    Ast::Program,
    (Ast::Operand, m_first)
    (Ast::Program::Operations, m_rest)
)

template <typename Iterator>
struct query_grammar
  : qi::grammar<Iterator, Ast::Program(), ascii::space_type>
{
  query_grammar()
    : query_grammar::base_type(expression)
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
    
    group       = lit('(') >> expression >> lit(')');
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

    factor      = group | term;
    expression  = factor >> *((char_('&') >> factor) | (char_('|') >> factor));

    group.name("group");
    term.name("term");
    factor.name("factor");
    expression.name("expression");

    debug(group);
    debug(term);
    debug(factor);
    debug(expression);
  }
  
  qi::rule<Iterator, Ast::Program(), ascii::space_type> expression;
  qi::rule<Iterator, Ast::Operand(), ascii::space_type> factor;
  qi::rule<Iterator, Ast::Term(), ascii::space_type> term;
  qi::rule<Iterator, Ast::Program(), ascii::space_type> group;

};

namespace Op
{
  class Operation;

  typedef boost::variant<
    Ast::Term,
    boost::recursive_wrapper< Operation >
    > Operand;

  class Operation : public std::pair<Operand, Operand>
  {
  public:
    friend std::ostream& operator<<(std::ostream &os, const Operation &p);

    enum Type {
      AND,
      OR,
    };

    Operation(Type type) : m_type(type) {}
    Operation(Type type, const Operand &first, const Operand &second) :
      std::pair<Operand, Operand>(first, second), m_type(type) {}

    Type type() const { return m_type; }

  private:
    Type m_type;
  };

  std::ostream& operator<<(std::ostream &os, const Operation &p)
  {
    switch (p.m_type) {
    case Operation::Type::AND:
      os << "AND";
      break;
    case Operation::Type::OR:
      os << "OR";
      break;
    default:
      throw std::runtime_error("Illegal opcode");
    }

    os << "(" << p.first << "," << p.second << ")";
    
    return os;
  }

};

class AstSolver : public boost::static_visitor<>
{
public:

  Op::Operand get() {
    assert(m_stack.size() == 1);

    return m_stack.top();
  }

  void operator()(const Ast::Term &term) {
    m_stack.push(Op::Operand(term));
  }

  void operator()(const Ast::Operation &o) {
    AstSolver solver;

    boost::apply_visitor(solver, o.m_operand);

    Op::Operation::Type optype;
    
    switch(o.m_type) {
    case '&':
      optype = Op::Operation::AND;
      break;
    case '|':
      optype = Op::Operation::OR;
      break;
    default:
      throw std::runtime_error("unexpected operation type");
    }

    Op::Operation op(optype, m_stack.top(), solver.get());
    
    m_stack.pop();
    m_stack.push(op);
  }

  void operator()(const Ast::Program &p) {
    AstSolver solver;

    boost::apply_visitor(solver, p.m_first);

    for (Ast::Program::Operations::const_iterator iter(p.m_rest.begin());
	 iter != p.m_rest.end(); ++iter)
      {
	Ast::Operand operand(*iter);

	boost::apply_visitor(solver, operand);
      }

    m_stack.push(solver.get());
  }

#if 0
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
#endif

private:
  typedef std::stack<Op::Operand> Stack;

  Stack          m_stack;
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

#if 0
  State::Strings _init = {
    "The quick brown fox jumped over the lazy dog",
    "Four score and seven years ago",
    "I have a dream"
  };
#endif

  using boost::spirit::ascii::space;
  typedef std::string::iterator iterator_type;
  typedef query_grammar<iterator_type> query_grammar;

  query_grammar g; // Our grammar

  Ast::Program result;

  bool r = phrase_parse(query.begin(), query.end(), g, space, result);

  AstSolver solver;
  Ast::Operand operand(result);

  boost::apply_visitor(solver, operand);

  std::cout << "query: "    << query
	    << " r: "       << r
	    << " results: " << result
	    << " solver: "  << solver.get()
	    << std::endl;

}
