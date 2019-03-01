#include <rai/mikron_storetool/block_helper.hpp>


rai::block_helper::block_type rai::block_helper::get_block_type (std::unique_ptr<rai::block> const & block, std::shared_ptr<rai::node> & node, rai::transaction & transaction)
{
	switch (block->type ())
	{
		case rai::block_type::state:
			{
				auto state_block (dynamic_cast<rai::state_block*> (block.get ()));
				rai::state_block_subtype subtype = node->ledger.state_subtype (transaction, *state_block);
				switch (subtype)
				{
					case rai::state_block_subtype::send: return block_type::state_send;
					case rai::state_block_subtype::receive: return block_type::state_receive;
					case rai::state_block_subtype::open_receive: return block_type::state_open_receive;
					case rai::state_block_subtype::open_genesis: return block_type::state_open_genesis;
					case rai::state_block_subtype::change: return block_type::state_change;
					default:
						assert (false);
						break;
				}
			}
			break;
		default:
			assert (false);
			break;
	}
	return block_type::undefined;
}

rai::block_hash rai::block_helper::get_receive_source (std::unique_ptr<rai::block> const & block, rai::block_helper::block_type block_type)
{
    switch (block_type)
	{
		case block_type::state_receive:
		case block_type::state_open_receive:
			{
				auto state_block (dynamic_cast<rai::state_block*> (block.get ()));
				return state_block->hashables.link;
			}
			break;
		case block_type::state_send:
		case block_type::state_open_genesis:
		case block_type::state_change:
			// No receive from
			return 0;
		default:
			assert (false);
			break;
	}
    return 0;
}
