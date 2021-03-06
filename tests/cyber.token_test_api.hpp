#pragma once
#include "test_api_helper.hpp"

namespace eosio { namespace testing {

struct recipient {
    name    to;
    asset   quantity;
    string  memo;
};

struct cyber_token_api: base_contract_api {
    cyber_token_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}

    symbol _symbol;

    void create_invoice_authority(account_name issuer, std::vector<name> invoicers) {
        authority auth(1, {});
        for (auto invoicer : invoicers) {
            auth.accounts.emplace_back(permission_level_weight{.permission = {invoicer, config::eosio_code_name}, .weight = 1});
        }
        if (std::find(invoicers.begin(), invoicers.end(), issuer) == invoicers.end()) {
            auth.accounts.emplace_back(permission_level_weight{.permission = {issuer, config::eosio_code_name}, .weight = 1});
        }
        std::sort(auth.accounts.begin(), auth.accounts.end(),
            [](const permission_level_weight& l, const permission_level_weight& r) {
                return std::tie(l.permission.actor, l.permission.permission) <
                    std::tie(r.permission.actor, r.permission.permission);
            });
        _tester->set_authority(issuer, golos::config::invoice_name, auth, "active");
    }

    //// token actions
    action_result create(account_name issuer, asset maximum_supply) {
        return push(N(create), _code, args()
            ("issuer", issuer)
            ("maximum_supply", maximum_supply)
        );
    }

    action_result issue(account_name issuer, account_name to, asset quantity, string memo = "") {
        return push(N(issue), issuer, args()
            ("to", to)
            ("quantity", quantity)
            ("memo", memo)
        );
    }

    action_result open(name owner) {
        return open(owner, _symbol, owner);
    }
    action_result open(account_name owner, symbol symbol, account_name payer = {}) {
        return push(N(open), owner, args()
            ("owner", owner)
            ("symbol", symbol)
            ("ram_payer", payer == name{} ? owner : payer)
        );
    }

    action_result close(account_name owner, symbol symbol) {
        return push(N(close), owner, args()
            ("owner", owner)
            ("symbol", symbol)
        );
    }

    action_result _transfer(action_name action, account_name from, account_name to, asset quantity, string memo) {
        return push(action, from, args()
            ("from", from)
            ("to", to)
            ("quantity", quantity)
            ("memo", memo)
        );
    }
    action_result transfer(account_name from, account_name to, asset quantity, string memo = "") {
        return _transfer(N(transfer), from, to, quantity, memo);
    }
    action_result payment(account_name from, account_name to, asset quantity, string memo = "") {
        return _transfer(N(payment), from, to, quantity, memo);
    }

    action_result claim(name owner, asset quantity) {
        return push(N(claim), owner, args()
            ("owner", owner)
            ("quantity", quantity));
    }

    action_result _bulk_transfer(action_name action, account_name from, std::vector<recipient> recipients) {
        return push(action, from, args()
            ("from", from)
            ("recipients", recipients));
    }
    action_result bulk_transfer(account_name from, std::vector<recipient> recipients) {
        return _bulk_transfer(N(bulktransfer), from, recipients);
    }
    action_result bulk_payment(account_name from, std::vector<recipient> recipients) {
        return _bulk_transfer(N(bulkpayment), from, recipients);
    }

    //// token tables
    variant get_stats() {
        auto sname = _symbol.to_symbol_code().value;
        auto v = get_struct(sname, N(stat), sname, "currency_stats");
        if (v.is_object()) {
            auto o = mvo(v);
            o["supply"] = o["supply"].as<asset>().to_string();
            o["max_supply"] = o["max_supply"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    variant get_account(account_name acc, bool raw_asset = false) {
        auto v = get_struct(acc, N(accounts), _symbol.to_symbol_code().value, "account");
        if (v.is_object() && !raw_asset) {
            auto o = mvo(v);
            o["balance"] = o["balance"].as<asset>().to_string();
            o["payments"] = o["payments"].as<asset>().to_string();
            v = o;
        }
        return v;
    }

    std::vector<variant> get_accounts(account_name user) {
        return _tester->get_all_chaindb_rows(_code, user, N(accounts), false);
    }

    //// helpers
    int64_t to_shares(double x) const {
        return x * _symbol.precision();
    }
    asset make_asset(double x = 0) const {
        return asset(to_shares(x), _symbol);
    }
    string asset_str(double x = 0) {
        return make_asset(x).to_string();
    }

    variant make_balance(double balance, double payments = 0) {
        return mvo()
            ("balance", asset_str(balance))
            ("payments", asset_str(payments));
    }

};


}} // eosio::testing
FC_REFLECT(eosio::testing::recipient, (to)(quantity)(memo))
