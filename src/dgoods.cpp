#include <dgoods.hpp>

ACTION dgoods::migratestats(const name category) {
    require_auth( get_self() );

    stats_index old_stats( get_self(), category.value );
    stats2_index new_stats( get_self(), category.value );

    config_index config_table(get_self(), get_self().value);
    auto config  = config_table.get();

    for ( auto old = old_stats.begin(); old != old_stats.end(); old++ ) {
        new_stats.emplace( get_self(), [&]( dgoodstats2& cat ) {
            cat.fungible = old->fungible;
            cat.burnable = old->burnable;
            cat.sellable = true;
            cat.transferable = old->transferable;
            cat.issuer = old->issuer;
            cat.rev_partner = get_self();
            cat.token_name = old->token_name;
            cat.category_name_id = old->category_name_id;
            cat.max_supply = asset(old->max_supply.amount, symbol(config.symbol, old->max_supply.precision));
            cat.current_supply = asset(old->current_supply, symbol(config.symbol, old->max_supply.precision));
            cat.issued_supply = asset(old->issued_supply, symbol(config.symbol, old->max_supply.precision));
            cat.rev_split = 0.0;
            cat.base_uri = old->base_uri;
        });
    }
}

ACTION dgoods::migrateaccs(const name owner, const uint64_t quantity) {
    require_auth( get_self() );

    account_index old_accounts( get_self(), owner.value );
    account2_index new_accounts( get_self(), owner.value );

    config_index config_table( get_self(), get_self().value );
    auto config  = config_table.get();

    auto old = old_accounts.begin();
    for( int i = 0; i < quantity && old != old_accounts.end(); i++ ) {
        new_accounts.emplace( get_self(), [&]( accounts2& acc) {
            acc.category_name_id = old->category_name_id;
            acc.category = old->category;
            acc.token_name = old->token_name;
            acc.amount = asset(old->amount.amount, symbol(config.symbol, old->amount.precision));
        });

        old++;
    }
}

ACTION dgoods::setconfig(const symbol_code& sym, const string& version) {

    require_auth( get_self() );
    // valid symbol
    check( sym.is_valid(), "not valid symbol" );

    // can only have one symbol per contract
    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get_or_create( get_self(), tokenconfigs{ "dgoods"_n, version, sym, 0 } );

    // setconfig will always update version when called
    config_singleton.version = version;
    config_table.set( config_singleton, get_self() );
}

ACTION dgoods::create(const name& issuer,
                      const name& rev_partner,
                      const name& category,
                      const name& token_name,
                      const bool& fungible,
                      const bool& burnable,
                      const bool& sellable,
                      const bool& transferable,
                      const double& rev_split,
                      const string& base_uri,
                      const asset& max_supply) {

    require_auth( get_self() );


    _checkasset( max_supply, fungible );
    // check if issuer account exists
    check( is_account( issuer ), "issuer account does not exist" );
    check( is_account( rev_partner), "rev_partner account does not exist" );
    // check split frac is between 0 and 1
    check( ( rev_split <= 1.0 ) && (rev_split >= 0.0), "rev_split must be between 0 and 1" );

    // get category_name_id
    config_index config_table( get_self(), get_self().value );
    check(config_table.exists(), "Symbol table does not exist, setconfig first");
    auto config_singleton  = config_table.get();
    auto category_name_id = config_singleton.category_name_id;


    category_index category_table( get_self(), get_self().value );
    auto existing_category = category_table.find( category.value );

    // category hasn't been created before, create it
    if ( existing_category == category_table.end() ) {
        category_table.emplace( get_self(), [&]( auto& cat ) {
            cat.category = category;
        });
    }

    asset current_supply = asset( 0, symbol( config_singleton.symbol, max_supply.symbol.precision() ));
    asset issued_supply = asset( 0, symbol( config_singleton.symbol, max_supply.symbol.precision() ));


    stats2_index stats2_table( get_self(), category.value );
    auto existing_token = stats2_table.find( token_name.value );
    check( existing_token == stats2_table.end(), "Token with category and token_name exists" );
    // token type hasn't been created, create it
    stats2_table.emplace( get_self(), [&]( auto& stats ) {
        stats.category_name_id = category_name_id;
        stats.issuer = issuer;
        stats.rev_partner= rev_partner;
        stats.token_name = token_name;
        stats.fungible = fungible;
        stats.burnable = burnable;
        stats.sellable = sellable;
        stats.transferable = transferable;
        stats.current_supply = current_supply;
        stats.issued_supply = issued_supply;
        stats.rev_split = rev_split;
        stats.base_uri = base_uri;
        stats.max_supply = max_supply;
    });

    // successful creation of token, update category_name_id to reflect
    config_singleton.category_name_id++;
    config_table.set( config_singleton, get_self() );
}


ACTION dgoods::issue(name to,
                     name category,
                     name token_name,
                     string quantity,
                     string relative_uri,
                     string memo) {

    check( is_account( to ), "to account does not exist");
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    // dgoodstats table
    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value,
                                               "Token with category and token_name does not exist" );

    // ensure have issuer authorization and valid quantity
    require_auth( dgood_stats.issuer );

    dasset q;
    if (dgood_stats.fungible == false) {
        // mint nft
        q.from_string("1", 0);
        // check cannot issue more than max supply, careful of overflow of uint
        check( q.amount <= (dgood_stats.max_supply.amount - dgood_stats.current_supply), "Cannot issue more than max supply" );

         mint(to, dgood_stats.issuer, category, token_name,
             dgood_stats.issued_supply, relative_uri, memo);
        add_balance(to, dgood_stats.issuer, category, token_name, dgood_stats.category_name_id, q);

    } else {
        // issue fungible
        q.from_string(quantity);
        // check cannot issue more than max supply, careful of overflow of uint
        check( q.amount <= (dgood_stats.max_supply.amount - dgood_stats.current_supply), "Cannot issue more than max supply" );
        string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
        check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
        add_balance(to, dgood_stats.issuer, category, token_name, dgood_stats.category_name_id, q);

    }

    // increase current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply += q.amount;
        s.issued_supply += q.amount;
    });

}

ACTION dgoods::burnnft(name owner,
                       vector<uint64_t> dgood_ids) {
    require_auth(owner);


    // loop through vector of dgood_ids, check token exists
    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );
        check( token.owner == owner, "must be token owner" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.burnable == true, "Not burnable");
        check( dgood_stats.fungible == false, "Cannot call burnnft on fungible token, call burn instead");

        dasset quantity;
        quantity.from_string("1", 0);

        // decrease current supply
        stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
            s.current_supply -= quantity.amount;
        });

        // lower balance from owner
        sub_balance(owner, dgood_stats.category_name_id, quantity);

        // erase token
        dgood_table.erase( token );
    }
}

ACTION dgoods::burnft(name owner,
                      uint64_t category_name_id,
                      string quantity) {
    require_auth(owner);


    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );

    stats_index stats_table( get_self(), acct.category.value );
    const auto& dgood_stats = stats_table.get( acct.token_name.value, "dgood stats not found" );

    dasset q;
    q.from_string(quantity);
    string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
    check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
    // lower balance from owner
    sub_balance(owner, category_name_id, q);

    // decrease current supply
    stats_table.modify( dgood_stats, same_payer, [&]( auto& s ) {
        s.current_supply -= q.amount;
    });


}


ACTION dgoods::transfernft(name from,
                           name to,
                           vector<uint64_t> dgood_ids,
                           string memo ) {
    // ensure authorized to send from account
    check( from != to, "cannot transfer to self" );
    require_auth( from );

    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    // loop through vector of dgood_ids, check token exists
    dgood_index dgood_table( get_self(), get_self().value );
    for ( auto const& dgood_id: dgood_ids ) {
        const auto& token = dgood_table.get( dgood_id, "token does not exist" );
        check( token.owner == from, "must be token owner" );

        stats_index stats_table( get_self(), token.category.value );
        const auto& dgood_stats = stats_table.get( token.token_name.value, "dgood stats not found" );

        check( dgood_stats.transferable == true, "not transferable");

        // notifiy both parties
        require_recipient( from );
        require_recipient( to );

        require_recipient( dgood_stats.issuer );

        dgood_table.modify( token, same_payer, [&] (auto& t ) {
            t.owner = to;
        });

        // amount 1, precision 0 for NFT
        dasset quantity;
        quantity.amount = 1;
        quantity.precision = 0;
        sub_balance(from, dgood_stats.category_name_id, quantity);
        add_balance(to, from, token.category, token.token_name, dgood_stats.category_name_id, quantity);
    }
}

ACTION dgoods::transferft(name from,
                          name to,
                          name category,
                          name token_name,
                          string quantity,
                          string memo ) {
    // ensure authorized to send from account
    check( from != to, "cannot transfer to self" );
    require_auth( from );


    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    require_recipient( from );
    require_recipient( to );

    stats_index stats_table( get_self(), category.value );
    const auto& dgood_stats = stats_table.get( token_name.value, "dgood stats not found" );
    check( dgood_stats.transferable == true, "not transferable");
    check( dgood_stats.fungible == true, "Must be fungible token");

    dasset q;
    q.from_string(quantity);
    string string_precision = "precision of quantity must be " + to_string(dgood_stats.max_supply.precision);
    check( q.precision == dgood_stats.max_supply.precision, string_precision.c_str() );
    sub_balance(from, dgood_stats.category_name_id, q);
    add_balance(to, from, category, token_name, dgood_stats.category_name_id, q);

}

ACTION dgoods::listsalenft(name seller,
                           uint64_t dgood_id,
                           asset net_sale_amount) {
    require_auth( seller );

    vector<uint64_t> dgood_ids = {dgood_id};

    check( net_sale_amount.amount > .02, "minimum price of at least 0.02 EOS");
    check( net_sale_amount.symbol == symbol( symbol_code("EOS"), 4), "only accept EOS for sale" );

    ask_index ask_table( get_self(), get_self().value );
    auto ask = ask_table.find( dgood_id);
    check ( ask == ask_table.end(), "already listed for sale" );

    // inline action checks ownership, permissions, and transferable prop
    SEND_INLINE_ACTION( *this, transfernft, { seller, name("active")},
                        {seller, get_self(), dgood_ids, "listing," + seller.to_string()} );

    // add token to table of asks
    ask_table.emplace( seller, [&]( auto& ask ) {
        ask.dgood_id = dgood_id;
        ask.seller = seller;
        ask.amount = net_sale_amount;
        ask.expiration = time_point_sec(current_time_point()) + WEEK_SEC;
    });
}

ACTION dgoods::closesalenft(name seller,
                            uint64_t dgood_id) {
    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( dgood_id, "cannot cancel sale that doesn't exist" );
    // if sale has expired anyone can call this and ask removed, token sent back to orig seller
    if ( time_point_sec(current_time_point()) > ask.expiration ) {
        ask_table.erase( ask );

        vector<uint64_t> dgood_ids = {dgood_id};
        // inline action checks ownership, permissions, and transferable prop
        SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                           {get_self(), ask.seller, dgood_ids, "clear sale returning to: " + ask.seller.to_string()} );
    } else {
        require_auth( seller );
        check( ask.seller == seller, "only the seller can cancel a sale in progress");

        vector<uint64_t> dgood_ids = {dgood_id};
        // inline action checks ownership, permissions, and transferable prop
        SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                           {get_self(), seller, dgood_ids, "close sale returning to: " + seller.to_string()} );
        ask_table.erase( ask );
    }
}

ACTION dgoods::buynft(name from,
                      name to,
                      asset quantity,
                      string memo) {
    // allow EOS to be sent by sending with empty string memo
    if ( memo == "deposit" ) return;
    // don't allow spoofs
    if ( to != get_self() ) return;
    if ( from == name("eosio.stake") ) return;
    if ( quantity.symbol != symbol( symbol_code("EOS"), 4) ) return;
    if ( memo.length() > 32 ) return;
    //memo format comma separated
    //dgood_id,to_account

    uint64_t dgood_id;
    name to_account;
    tie( dgood_id, to_account ) = parsememo(memo);

    ask_index ask_table( get_self(), get_self().value );
    const auto& ask = ask_table.get( dgood_id, "token not listed for sale" );
    check ( ask.amount.amount == quantity.amount, "send the correct amount");
    check (ask.expiration > time_point_sec(current_time_point()), "sale has expired");

    action( permission_level{ get_self(), name("active") }, name("eosio.token"), name("transfer"),
            make_tuple( get_self(), ask.seller, quantity, "sold token: " + to_string(dgood_id))).send();

    ask_table.erase(ask);

    // inline action checks ownership, permissions, and transferable prop
    vector<uint64_t> dgood_ids = {dgood_id};
    SEND_INLINE_ACTION( *this, transfernft, { get_self(), name("active")},
                        {get_self(), to_account, dgood_ids, "bought by: " + to_account.to_string()} );

}

// method to log dgood_id and match transaction to action
ACTION dgoods::logcall(uint64_t dgood_id) {
    require_auth( get_self() );
}

// method to notify issuer of the new nft id
ACTION dgoods::logissuenft(name issuer,
                           uint64_t dgood_id,
                           string memo) {
    require_auth( get_self() );
    require_recipient( issuer );
}

// Private
void dgoods::_checkasset(const asset& amount, const bool& fungible) {
    auto sym = amount.symbol;
    if (fungible) {
        check( amount.amount > 0, "amount must be positive" );
    } else {
        check( sym.precision() == 0, "NFT must be an int, precision of 0" );
        check( amount.amount >= 1, "NFT amount must be >= 1" );
    }

    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get();
    check( config_singleton.symbol.raw() == sym.code().raw(), "Symbol must match symbol in config" );
    check( amount.is_valid(), "invalid amount" );
}

// Private
void dgoods::mint(name to,
                  name issuer,
                  name category,
                  name token_name,
                  uint64_t issued_supply,
                  string relative_uri,
                  string memo) {

    dgood_index dgood_table( get_self(), get_self().value);
    auto dgood_id = dgood_table.available_primary_key();
    if ( relative_uri.empty() ) {
        dgood_table.emplace( issuer, [&]( auto& dg) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply + 1;
            dg.owner = to;
            dg.category = category;
            dg.token_name = token_name;
            dg.relative_uri = token_name.to_string() + "-" +  to_string(issued_supply + 1) + ".json";
        });
    } else {
        dgood_table.emplace( issuer, [&]( auto& dg ) {
            dg.id = dgood_id;
            dg.serial_number = issued_supply + 1;
            dg.owner = to;
            dg.category = category;
            dg.token_name = token_name;
            dg.relative_uri = relative_uri;
        });
    }
    SEND_INLINE_ACTION( *this, logcall, { { get_self(), "active"_n } }, { dgood_id } );
    SEND_INLINE_ACTION( *this, logissuenft, { { get_self(), "active"_n } }, { issuer, dgood_id, memo } );
}

// Private
void dgoods::add_balance(name owner, name ram_payer, name category, name token_name,
                         uint64_t category_name_id, dasset quantity) {
    account_index to_account( get_self(), owner.value );
    auto acct = to_account.find( category_name_id );
    if ( acct == to_account.end() ) {
        to_account.emplace( ram_payer, [&]( auto& a ) {
            a.category_name_id = category_name_id;
            a.category = category;
            a.token_name = token_name;
            a.amount = quantity;
        });
    } else {
        to_account.modify( acct, same_payer, [&]( auto& a ) {
            a.amount.amount += quantity.amount;
        });
    }
}

// Private
void dgoods::sub_balance(name owner, uint64_t category_name_id, dasset quantity) {

    account_index from_account( get_self(), owner.value );
    const auto& acct = from_account.get( category_name_id, "token does not exist in account" );
    check( acct.amount.amount >= quantity.amount, "quantity is more than account balance");

    if ( acct.amount.amount == quantity.amount ) {
        from_account.erase( acct );
    } else {
        from_account.modify( acct, same_payer, [&]( auto& a ) {
            a.amount.amount -= quantity.amount;
        });
    }
}

extern "C" {
    void apply (uint64_t receiver, uint64_t code, uint64_t action ) {
        auto self = receiver;

        if ( code == self ) {
            switch( action ) {
                EOSIO_DISPATCH_HELPER( dgoods, (migratestats)(migrateaccs)(setconfig)(create)(issue)(burnnft)(burnft)(transfernft)(transferft)(listsalenft)(closesalenft)(logcall)(logissuenft) )
            }
        }

        else {
            if ( code == name("eosio.token").value && action == name("transfer").value ) {
                execute_action( name(receiver), name(code), &dgoods::buynft );
            }
        }
    }
}

