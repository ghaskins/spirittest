
#include <iostream>
#include <list>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  int ret;
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produces help message")
    ;

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
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

}


