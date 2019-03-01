#include <boost/filesystem.hpp>
#include <map>
#include <rai/lib/numbers.hpp>
#include <rai/node/node.hpp>
#include <set>
#include <vector>

namespace rai
{
enum class state_block_subtype : uint8_t
{
	undefined = 0,
	send = 2,
	receive = 3,
	open_receive = 4,
	open_genesis = 5,
	change = 6
};

class state_block_subtype_helper
{
public:
	static rai::state_block_subtype get_subtype (rai::state_block const &, rai::uint128_t);
	static rai::state_block_subtype get_subtype (rai::ledger &, MDB_txn *, rai::state_block const &);
};

class span_info
{
public:
	rai::block_hash hash;
	std::set <rai::account> accounts;
	std::set <rai::block_hash> blocks;
	void merge (rai::span_info &);
	void merge_acc (const rai::account &);
	void merge_block (const rai::block_hash &);
};

class block_span_analyzer
{
public:
	static rai::state_block_subtype get_subtype (rai::ledger const &);

	void analyze (boost::filesystem::path);
	span_info analyze_block (rai::block_hash const &, int rec_level, int& n_checks, int& n_checks2);
	span_info get_block_span (rai::block_hash const &, int rec_level, int& n_checks, int& n_checks2);
private:
	std::shared_ptr <rai::node> node;
	rai::transaction * transaction;
	std::vector <rai::block_hash> blocks;
	std::map <rai::block_hash, span_info> span_map;
};
}
