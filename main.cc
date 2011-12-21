
#include <iostream>
#include <sstream>
#include <ios>
#include <list>
#include <assert.h>
#include <time.h>
#include <boost/program_options.hpp>

#include "orderbook.hh"

using namespace Exchange;

namespace po = boost::program_options;

class TradeMonitor : public Instrument::Monitor {
public:
  TradeMonitor() : m_trades(0), m_submits(0), m_cancels(0) {}

  void onTrade(Trade::ExecutionReport report) {
    m_trades++;
  }

  void onSubmit(Ref ref, Order::Type type, 
		Order::Direction dir, Price price, Quantity quantity) {
    m_submits++;
  }

  void onCancel(Ref ref, Quantity quantity) {
    m_cancels++;
  }

  int m_trades;
  int m_submits;
  int m_cancels;
};

class StdoutTradeMonitor : public TradeMonitor {
  void onTrade(Trade::ExecutionReport report) {
    TradeMonitor::onTrade(report);
    std::cout << "Trade: " << report << std::endl;
  }

  void onSubmit(Ref ref, Order::Type type, 
		Order::Direction dir, Price price, Quantity quantity) {
    TradeMonitor::onSubmit(ref, type, dir, price, quantity);
    std::cout << "Submit ->"
	      << " R:" << ref
	      << " T:" << (type == Order::MARKET ? "MARKET" : "LIMIT")
	      << " D:" << (dir == Order::CALL ? "CALL" : "PUT")
	      << " P:" << price/100 << "." << price%100
	      << " Q:" << quantity
	      << std::endl;
  }

  void onCancel(Ref ref, Quantity quantity) {
    TradeMonitor::onCancel(ref, quantity);
    std::cout << "Cancel ->"
	      << " R:" << ref
	      << " Q:" << quantity
	      << std::endl;
  }
};

class Random {
public:
  Random(int min, int max) {
    init(min, max);
  }
  Random() {
    init(0, RAND_MAX);
  }

  friend std::ostream& operator<<(std::ostream& out, Random random);

  int get() { return m_val; }

private:
  void init(int min, int max) {
    m_val = (random() % (max-min+1)) + min;
  }

  int m_val;
};

std::ostream& operator<<(std::ostream& out, Random random)
{
  out << random.m_val;
}

typedef long long Time;

Time
ts2nsec(timespec ts)
{
  Time val(0);

  val = ts.tv_sec * 1000000000;
  val += ts.tv_nsec;

  return val;
}

Time
deltatime(Time start, Time end)
{
  return end-start;
}

class TestInstrument : public Instrument {
public:
  TestInstrument(Ref &ref, Symbol symbol, Instrument::Monitor *mon, unsigned limitcount) :
    Instrument(symbol, mon) {
 
    // pre-populate the books with some standing limit orders
    for (unsigned i(0); i<limitcount/2; i++) 
      {
	Random price(1000, 1030);
	Random quantity(100, 1000);
	
	this->submitOrder(Order(ref++, Order::CALL, Order::LIMIT,
				price.get(), quantity.get()));
      }
    
    for (unsigned i(0); i<limitcount/2; i++) 
      {
	Random price(1040, 1060);
	Random quantity(100, 1000);
	
	this->submitOrder(Order(ref++, Order::PUT, Order::LIMIT,
				price.get(), quantity.get()));
      }
  }
  
};

int ipow(int base, int exp)
{
   int pow = base;
   int v = 1;
   if (exp < 0) {
      assert (base != 0);  /* divide by zero */
      return (base*base != 1)? 0: (exp&1)? base : 1;
   }
 
   while(exp > 0 )
   {
      if (exp & 1) v *= pow;
      pow *= pow;
      exp >>= 1;
   }
   return v;
}

std::string num2sym(unsigned int num) {
  int base(26);
  int size(4);
  std::ostringstream os;

  for(int i(size-1); i>=0; i--) {
    int factor(ipow(base, i));
    int digit(num/factor);

    os << std::string(1, (char)('a' + digit));

    num -= (digit*factor);
  }

  return os.str();
}

class MatchingEngine {
  typedef std::map<Symbol, Instrument*> InstrumentMap;
public:
  MatchingEngine(Instrument::Monitor *mon, unsigned symbolcount, unsigned limitcount) :
    m_mon(mon), m_ref(0), m_limitcount(limitcount)
  {
    for (int i(0); i<symbolcount; i++) {
      addInstrument(num2sym(i));
    }
  }
  ~MatchingEngine() {
    for (InstrumentMap::iterator iter(m_instruments.begin());
	 iter != m_instruments.end();
	 iter++) {
      delete iter->second;
    }
  }

  void submitOrder(const Symbol &symbol, Order order) {
    order.m_ref = m_ref++;

    InstrumentMap::const_iterator iter(m_instruments.find(symbol));

    assert(iter != m_instruments.end());
    iter->second->submitOrder(order);
  }

private:
  void addInstrument(Symbol symbol) {
    m_instruments[symbol] = new TestInstrument(m_ref, symbol, m_mon, m_limitcount);    
  }

  Instrument::Monitor *m_mon;
  Ref m_ref;
  InstrumentMap m_instruments;
  unsigned m_limitcount;
};

int main(int argc, char **argv) {
  int ret;
  unsigned ordercount(100000);
  unsigned symbolcount(10000);
  unsigned limitcount(10);
  

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produces help message")
    ("orders,o", po::value<unsigned>(&ordercount),
     "the number of orders")
    ("instruments,i", po::value<unsigned>(&symbolcount),
     "the number of instruments to trade")
    ("limits,l", po::value<unsigned>(&limitcount),
     "the number of limit orders to prepopulate on each side of each book")
    ;

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cerr << desc << std::endl;
    return -1;
  }

  std::cout << "Running exchange simulation with "
	    << ordercount << " orders, "
	    << symbolcount << " instruments, and "
	    << limitcount << " pre-populated limit orders per book per side"
	    << std::endl;

  //TradeMonitor *mon(new StdoutTradeMonitor());
  TradeMonitor *mon(new TradeMonitor());
  MatchingEngine me(mon, symbolcount, limitcount);

  typedef std::pair<Symbol, Order> TestOrder;
  typedef std::list<TestOrder> OrderList;
  OrderList orders;

  // precompute the orders so that we dont measure the random number generation
  // and other peripheral tasks
  for (int i(0); i<ordercount; i++)
    {
      Order::Direction dir((Order::Direction)Random(Order::CALL, Order::PUT).get());
      Order::Type type((Order::Type)Random(Order::MARKET, Order::LIMIT).get());
      Random quantity(1, 1000);
      Price price(0);
      Random symbol(0, symbolcount-1);

      if (type == Order::LIMIT)
	price = Random(1000, 1060).get();

      // Ref will get filled in later
      TestOrder order(num2sym(symbol.get()), Order(Ref(0), dir, type,
						   price, quantity.get()));
      orders.push_back(order);
    }
  
  // now start the actual trading simulation
  timespec stime, etime;
  clock_gettime(CLOCK_MONOTONIC, &stime);

  for (OrderList::iterator iter(orders.begin());
       iter != orders.end();
       ++iter)
    {
      me.submitOrder(iter->first, iter->second);
    }

  clock_gettime(CLOCK_MONOTONIC, &etime);

  Time delta(deltatime(ts2nsec(stime), ts2nsec(etime)));

  std::cout
    << "Summary ->"
    << " Submits: " << mon->m_submits
    << " Trades:" << mon->m_trades
    << " Cancels: " << mon->m_cancels
    << " ns/trade: " << delta/ordercount
    << std::endl;

  delete mon;
}


