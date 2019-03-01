#include <rai/mikron_storetool/block_span_analyzer.hpp>
#include <rai/node/node.hpp>
#include <random>


rai::state_block_subtype rai::state_block_subtype_helper::get_subtype (rai::state_block const & block_a, rai::uint128_t previous_balance_a)
{
	// if there is no previous: open
	if (block_a.hashables.previous.is_zero ())
	{
		if (!block_a.hashables.link.is_zero ())
		{
			// no prev but link: open_receive
			return rai::state_block_subtype::open_receive;
		}
		else
		{
			// no prev, no link: open_genesis
			return rai::state_block_subtype::open_genesis;
		}
	}
	// has previous, has previous balance
	// check balances: if decreasing: send
	auto cur_balance (block_a.hashables.balance.number ());
	if (cur_balance < previous_balance_a)
	{
		return rai::state_block_subtype::send;
	}
	// if balance increasing: receive
	if (cur_balance > previous_balance_a)
	{
		return rai::state_block_subtype::receive;
	}
	// balance does not change, and no link: change
	if (block_a.hashables.link.is_zero ())
	{
		return rai::state_block_subtype::change;
	}
	// otherwise: undefined, which is not a valid block
	return rai::state_block_subtype::undefined;
}

rai::state_block_subtype rai::state_block_subtype_helper::get_subtype (rai::ledger & ledger, MDB_txn * transaction_a, rai::state_block const & block_a)
{
	rai::block_hash previous_hash (block_a.hashables.previous);
	if (previous_hash.is_zero ())
	{
		return get_subtype (block_a, 0); // no previous block -- in this case time does not matter
	}
	auto previous_block (ledger.store.block_get (transaction_a, previous_hash));
	assert (previous_block != nullptr);
	if (previous_block == nullptr) return rai::state_block_subtype::undefined;
	auto previous_balance = ledger.balance (transaction_a, previous_hash); // could be retrieved directly from the block
	return get_subtype (block_a, previous_balance);
}

void rai::span_info::merge (rai::span_info & span2)
{
	std::set<rai::account>::iterator it;
	for (it = span2.accounts.begin (); it != span2.accounts.end (); ++it)
	{
		merge_acc (*it);
	}
	std::set<rai::block_hash>::iterator itb;
	for (itb = span2.blocks.begin (); itb != span2.blocks.end (); ++itb)
	{
		merge_block (*itb);
	}
}

void rai::span_info::merge_acc (const rai::account & account)
{
	if (accounts.count (account) == 0)
	{
		accounts.insert (account);
	}
}

void rai::span_info::merge_block (const rai::block_hash & block)
{
	if (blocks.count (block) == 0)
	{
		blocks.insert (block);
	}
}

void rai::block_span_analyzer::analyze (boost::filesystem::path data_path)
{
	std::cout << "debug_block_span " << std::endl;
	rai::inactive_node * node_a = new rai::inactive_node (data_path);
	node = node_a->node;
	transaction = new rai::transaction (node->store.environment, false);
	//blocks = pick_random_blocks (100, node);
	span_map = std::map<rai::block_hash, span_info> ();
	//int n = blocks.size ();
	//std::map<rai::block_hash, rai::span_info> account_spans;
	std::vector<int> account_span_account_counts;
	std::vector<int> account_span_blocks_counts;
	int cnt = 0;
	for (auto i (node->store.latest_begin (*transaction)), n (node->store.latest_end ()); i != n; ++i, ++cnt)
	{
		std::cout << "i_cnt " << cnt << " "; // std::endl;
		rai::account account = rai::account (i->first);
		rai::account_info account_info = rai::account_info (i->second);
		rai::block_hash block_hash = account_info.head;
		int n_checks = 0;
		int n_checks2 = 0;
		span_info span = get_block_span (block_hash, 0, n_checks, n_checks2);
		//account_spans[block_hash] = span;
		account_span_account_counts.push_back (span.accounts.size ());
		account_span_blocks_counts.push_back (span.blocks.size ());
		std::cout << std::endl
				  << "i_cnt " << cnt << " acc " << span.accounts.size () << " bl " << span.blocks.size () << " " << account.to_account () << std::endl;
	}
	// computing medians
	std::sort (account_span_account_counts.begin (), account_span_account_counts.end ());
	int median_account_count = account_span_account_counts[account_span_account_counts.size () / 2];
	std::cout << "median_account_count " << median_account_count << " " << account_span_account_counts[0] << " - " << account_span_account_counts[account_span_account_counts.size () - 1] << std::endl;
	std::sort (account_span_blocks_counts.begin (), account_span_blocks_counts.end ());
	int median_block_count = account_span_blocks_counts[account_span_blocks_counts.size () / 2];
	std::cout << "median_block_count " << median_block_count << " " << account_span_blocks_counts[0] << " - " << account_span_blocks_counts[account_span_blocks_counts.size () - 1] << std::endl;
}

rai::span_info rai::block_span_analyzer::analyze_block (rai::block_hash const & hash, int rec_level, int& n_checks, int& n_checks2)
{
	rai::span_info span;
	if (rec_level >= 100)
		return span;
	//printf("hash %d %s\n", rec_level, hash.to_string().c_str());
	auto block (node->store.block_get (*transaction, hash));
	//assert (block != nullptr);
	if (block == nullptr)
	{
		printf("block == nullptr\n");
		return span;
	}
	span.merge_block (hash);
	++n_checks;
	++n_checks2;
	rai::block_hash prev = block->previous ();
	if (prev.number () != 0)
	{
		rai::span_info prev_span = get_block_span (prev, rec_level+1, n_checks, n_checks2);
		span = prev_span;
		span.merge_block (hash);
	}
	switch (block->type ())
	{
		case rai::block_type::state:
			{
				auto state_block (dynamic_cast<rai::state_block *> (block.get ()));
				span.merge_acc (state_block->hashables.account);
				rai::state_block_subtype subtype = rai::state_block_subtype_helper::get_subtype(node->ledger, *transaction, *state_block);
				if (subtype == rai::state_block_subtype::receive || subtype == rai::state_block_subtype::open_receive)
				{
					// receive
					rai::block_hash send_hash = state_block->hashables.link;
					//auto block (node->store.block_get (*transaction, send_hash));
					span_info send_span = get_block_span (send_hash, rec_level+1, n_checks, n_checks2);
					span.merge (send_span);
				}
			}
			break;
		case rai::block_type::open:
			{
				auto open_block (dynamic_cast<rai::open_block *> (block.get ()));
				span.merge_acc (open_block->hashables.account);
				rai::block_hash send_hash = open_block->hashables.source;
				//auto block (node->store.block_get (*transaction, send_hash));
				span_info send_span = get_block_span (send_hash, rec_level+1, n_checks, n_checks2);
				span.merge (send_span);
			}
			break;
		case rai::block_type::receive:
			{
				auto receive_block (dynamic_cast<rai::receive_block *> (block.get ()));
				// receive block has no account, strange, take it from ledger
				// receive_block->hashables.account
				rai::account account = node->ledger.account(*transaction, hash);
				span.merge_acc (account);
				rai::block_hash send_hash = receive_block->hashables.source;
				//auto block (node->store.block_get (*transaction, send_hash));
				span_info send_span = get_block_span (send_hash, rec_level+1, n_checks, n_checks2);
				span.merge (send_span);
			}
			break;
		case rai::block_type::send:
		case rai::block_type::change:
		case rai::block_type::not_a_block:
		case rai::block_type::invalid:
			// do not follow these blocks
			break;
		default:
			printf("Unsupported type %d %d\n", (int)block->type (), rec_level);
			break;
	}
	return span;
}

rai::span_info rai::block_span_analyzer::get_block_span (rai::block_hash const & hash, int rec_level, int& n_checks, int& n_checks2)
{
	if (hash.number () == 0)
	{
		return rai::span_info ();
	}
	if (span_map.count (hash) > 0)
	{
		// found in cache
		std::cerr << std::endl << "found in cache l " << rec_level << " ch " << n_checks << " " << n_checks2 << " cs " << span_map.size () << " b " << span_map[hash].blocks.size () << " ";
		n_checks2 = 0;
		return span_map[hash];
	}
	rai::span_info span = analyze_block (hash, rec_level, n_checks, n_checks2);
	if (n_checks2 >= 20)
	{
		span_map[hash] = span;
		//std::cerr << "added span " << rec_level << " a " << span.accounts.size () << " b " << span.blocks.size () << " cs " << span_map.size () << " " << hash.to_string () << std::endl;
	}
	if (rec_level <= 20) std::cerr << rec_level << ".";
	else if (rec_level <= 100) std::cerr << ".";
	return span;
}
