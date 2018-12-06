#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {
struct golos_charge_api: base_contract_api {
    golos_charge_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    action_result set_restorer(name issuer, uint8_t charge_id, std::string func_str,
        int64_t max_prev, int64_t max_vesting, int64_t max_elapsed) {
        return push(N(setrestorer), issuer, args()
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("func_str", func_str)
            ("max_prev", max_prev)
            ("max_vesting", max_vesting)
            ("max_elapsed", max_elapsed)
        );
    }

    action_result use(name issuer, name user, uint8_t charge_id, int64_t price, int64_t cutoff) {
        return push(N(use), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("price", price)
            ("cutoff", cutoff)
        );
    }
    
    action_result use_and_store(name issuer, name user, uint8_t charge_id, int64_t stamp_id, int64_t price) {
        return push(N(useandstore), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("stamp_id", stamp_id)
            ("price", price)
        );
    }
    
    action_result remove_stored(name issuer, name user, uint8_t charge_id, int64_t stamp_id) {
        return push(N(removestored), issuer, args()
            ("user", user)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("stamp_id", stamp_id)
        );
    }
};


}} // eosio::testing