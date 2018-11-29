#include "golos.social.hpp"

namespace golos {

using namespace eosio;

extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        social(receiver).apply(code, action);
    }
}

social::social(account_name self)
    : contract(self)
{}

void social::apply(uint64_t code, uint64_t action) {
    if (N(pin) == action) {
        execute_action(this, &social::pin);
    }
    if (N(unpin) == action) {
        execute_action(this, &social::unpin);
    }
    if (N(block) == action) {
        execute_action(this, &social::block);
    }
    if (N(unblock) == action) {
        execute_action(this, &social::unblock);
    }
}

void social::pin(account_name pinner, account_name pinning) {
    require_auth(pinner);

    eosio_assert(pinner != pinning, "You cannot pin yourself");

    tables::pinblock_table table(_self, pinner);
    auto itr = table.find(pinning);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio_assert(!itr->blocking, "You have blocked this account. Unblock it before pinning");
        eosio_assert(!itr->pinning, "You already have pinned this account");

        table.modify(itr, 0, [&](auto& item){
            item.pinning = true;
        });

        return;
    }

    table.emplace(pinner, [&](auto& item){
        item.account = pinning;
        item.pinning = true;
    });
}

void social::unpin(account_name pinner, account_name pinning) {
    require_auth(pinner);

    eosio_assert(pinner != pinning, "You cannot unpin yourself");

    tables::pinblock_table table(_self, pinner);
    auto itr = table.find(pinning);
    bool item_exists = (itr != table.end());

    eosio_assert(item_exists && itr->pinning, "You have not pinned this account");

    table.modify(itr, 0, [&](auto& item){
        item.pinning = false;
    });
}

void social::block(account_name blocker, account_name blocking) {
    require_auth(blocker);

    eosio_assert(blocker != blocking, "You cannot block yourself");

    tables::pinblock_table table(_self, blocker);
    auto itr = table.find(blocking);
    bool item_exists = (itr != table.end());

    if (item_exists) {
        eosio_assert(!itr->blocking, "You already have blocked this account");

        table.modify(itr, 0, [&](auto& item){
            item.pinning = false;
            item.blocking = true;
        });

        return;
    }

    table.emplace(blocker, [&](auto& item){
        item.account = blocking;
        item.blocking = true;
    });
}

void social::unblock(account_name blocker, account_name blocking) {
    require_auth(blocker);

    eosio_assert(blocker != blocking, "You cannot unblock yourself");

    tables::pinblock_table table(_self, blocker);
    auto itr = table.find(blocking);
    bool item_exists = (itr != table.end());

    eosio_assert(item_exists && itr->blocking, "You have not blocked this account");

    table.modify(itr, 0, [&](auto& item){
        item.blocking = false;
    });
}


} // golos