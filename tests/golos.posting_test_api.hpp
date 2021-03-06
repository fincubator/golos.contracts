#pragma once
#include "test_api_helper.hpp"
#include "golos.publication_rewards_types.hpp"
#include <contracts.hpp>

namespace eosio { namespace testing {

namespace cfg = golos::config;

struct golos_posting_api: base_contract_api {
    golos_posting_api(golos_tester* tester, name code, symbol sym)
    :   base_contract_api(tester, code)
    ,   _symbol(sym) {}
    symbol _symbol;

    void initialize_contract(name token_name, name charge_name) {
        _tester->install_contract(_code, contracts::posting_wasm(), contracts::posting_abi());

        _tester->set_authority(_code, cfg::code_name, create_code_authority({_code}), "active");
        _tester->link_authority(_code, _code, cfg::code_name, N(closemssgs));
        _tester->link_authority(_code, _code, cfg::code_name, N(paymssgrwrd));
        _tester->link_authority(_code, _code, cfg::code_name, N(deletevotes));
        _tester->link_authority(_code, token_name, cfg::code_name, N(transfer));
        _tester->link_authority(_code, token_name, cfg::code_name, N(payment));

        name action = "calcrwrdwt"_n;
        auto auth = authority(1, {}, {
            {.permission = {charge_name, config::eosio_code_name}, .weight = 1}
        });
        _tester->set_authority(_code, action, auth, "active");
        _tester->link_authority(_code, _code, action, action);
        _tester->link_authority(_code, token_name, cfg::code_name, N(bulktransfer));
        _tester->link_authority(_code, token_name, cfg::code_name, N(bulkpayment));
    }

    //// posting actions
    action_result set_rules(
        const funcparams& main_fn,
        const funcparams& curation_fn,
        const funcparams& time_penalty,
        uint16_t max_token_prop,
        std::optional<symbol> token_symbol = std::optional<symbol>()) {
        return push(N(setrules), _code, args()
            ("mainfunc", fn_to_mvo(main_fn))
            ("curationfunc", fn_to_mvo(curation_fn))
            ("timepenalty", fn_to_mvo(time_penalty))
            ("maxtokenprop", max_token_prop)
            ("tokensymbol", token_symbol.value_or(_symbol))
        );
    }
    
    action_result syncpool() {
        return push(N(syncpool), _code, args());
    }

    action_result set_limit(std::string act, uint8_t charge_id = 0, int64_t price = -1, int64_t cutoff = 0, int64_t vesting_price = 0, int64_t min_vesting = 0) {
        return push(N(setlimit), _code, args()
            ("act", act)
            ("token_code", _symbol.to_symbol_code())
            ("charge_id", charge_id)
            ("price", price)
            ("cutoff", cutoff)
            ("vesting_price", vesting_price)
            ("min_vesting", min_vesting)
        );
    }

    action_result create_msg(
        mssgid message_id,
        mssgid parent_id = {N(), "parentprmlnk"},
        std::vector<beneficiary> beneficiaries = {},
        uint16_t token_prop = 5000,
        bool vest_payment = false,
        std::string title = "headermssg",
        std::string body = "bodymssg",
        std::string language = "languagemssg",
        std::vector<std::string> tags = {"tag"},
        std::string json_metadata = "jsonmetadata",
        optional<uint16_t> curators_prcnt = optional<uint16_t>(),
        optional<asset> max_payout = optional<asset>()
    ) {
        return push(N(createmssg), message_id.author, args()
            ("message_id", message_id)
            ("parent_id", parent_id)
            ("beneficiaries", beneficiaries)
            ("tokenprop", token_prop)
            ("vestpayment", vest_payment)
            ("headermssg", title)
            ("bodymssg", body)
            ("languagemssg", language)
            ("tags", tags)
            ("jsonmetadata", json_metadata)
            ("curators_prcnt", curators_prcnt)
            ("max_payout", max_payout)
        );
    }

    action_result reblog_msg(
        account_name rebloger,
        mssgid message_id,
        std::string title = "headermssg",
        std::string body = "bodymssg"
    ) {
        return push(N(reblog), rebloger, args()
            ("rebloger", rebloger)
            ("message_id", message_id)
            ("headermssg", title)
            ("bodymssg", body)
        );
    }

    action_result erase_reblog_msg(
        account_name rebloger,
        mssgid message_id
    ) {
        return push(N(erasereblog), rebloger, args()
            ("rebloger", rebloger)
            ("message_id", message_id)
        );
    }

    action_result update_msg(
        mssgid message_id,
        std::string title,
        std::string body,
        std::string language,
        std::vector<std::string> tags,
        std::string json_metadata
    ) {
        return push(N(updatemssg), message_id.author, args()
            ("message_id", message_id)
            ("headermssg", title)
            ("bodymssg", body)
            ("languagemssg", language)
            ("tags",tags)
            ("jsonmetadata", json_metadata)
        );
    }

    action_result delete_msg(mssgid message_id) {
        return push(N(deletemssg), message_id.author, args()
            ("message_id", message_id)
        );
    }

    action_result upvote(account_name voter, mssgid message_id, uint16_t weight = cfg::_100percent) {
        return push(N(upvote), voter, args()
            ("voter", voter)
            ("message_id", message_id)
            ("weight", weight)
        );
    }
    action_result downvote(account_name voter, mssgid message_id, uint16_t weight = cfg::_100percent) {
        return push(N(downvote), voter, args()
            ("voter", voter)
            ("message_id", message_id)
            ("weight", weight)
        );
    }
    action_result unvote(account_name voter, mssgid message_id) {
        return push(N(unvote), voter, args()
            ("voter", voter)
            ("message_id", message_id)
        );
    }
    action_result set_curators_prcnt(mssgid message_id, uint16_t curators_prcnt) {
        return push(N(setcurprcnt), message_id.author, args()
            ("message_id", message_id)
            ("curators_prcnt", curators_prcnt)
            );
    }
    action_result set_max_payout(mssgid message_id, asset max_payout) {
        return push(N(setmaxpayout), message_id.author, args()
            ("message_id", message_id)
            ("max_payout", max_payout)
            );
    }
    action_result set_params(std::string json_params) {
        return push(N(setparams), _code, args()
            ("params", json_str_to_obj(json_params)));
    }
    action_result closemssgs(account_name payer) {
        return push(N(closemssgs), payer, args()("payer", payer));
    }
    action_result closemssgs() {
        return closemssgs(_code);
    }

    action_result add_permlink(mssgid msg, mssgid parent, uint16_t level = 0, uint32_t childcount = 0) {
        return push(N(addpermlink), _code, args()
            ("msg", msg)
            ("parent", parent)
            ("level", level)
            ("childcount", childcount)
        );
    }
    action_result del_permlink(mssgid msg) {
        return push(N(delpermlink), _code, args()("msg", msg));
    }
    action_result add_permlinks(std::vector<permlink_info> permlinks) {
        return push(N(addpermlinks), _code, args()("permlinks", permlinks));
    }
    action_result del_permlinks(std::vector<mssgid> permlinks) {
        return push(N(delpermlinks), _code, args()("permlinks", permlinks));
    }


    action_result init_default_params() {
        auto vote_changes = get_str_vote_changes(max_vote_changes);
        auto cashout_window = get_str_cashout_window(window, upvote_lockout);
        auto beneficiaries = get_str_beneficiaries(max_beneficiaries);
        auto comment_depth = get_str_comment_depth(max_comment_depth);
        auto social = get_str_social_acc(name());
        auto referral = get_str_referral_acc(name());
        auto curators_prcnt = get_str_curators_prcnt(min_curators_prcnt, max_curators_prcnt);
        auto bwprovider = get_str_bwprovider(name(), name());
        auto min_abs_rshares = get_str_min_abs_rshares_param(8);

        auto params = "[" + vote_changes + "," + cashout_window + "," + beneficiaries + "," + comment_depth +
            "," + social + "," + referral + "," + curators_prcnt + "," + bwprovider + "," + min_abs_rshares + "]";
        return set_params(params);
    }

    variant get_params() const {
        return base_contract_api::get_struct(_code, N(pstngparams), N(pstngparams), "posting_state");
    }

    string get_str_vote_changes(uint8_t max_vote_changes) {
        return string("['st_max_vote_changes', {'value':'") + std::to_string(max_vote_changes) + "'}]";
    }

    string get_str_cashout_window(uint32_t window, uint32_t upvote_lockout) {
        return string("['st_cashout_window', {'window':'") + std::to_string(window) + "','upvote_lockout':'" + std::to_string(upvote_lockout) + "'}]";
    }

    string get_str_beneficiaries(uint8_t max_beneficiaries) {
        return string("['st_max_beneficiaries', {'value':'") + std::to_string(max_beneficiaries) + "'}]";
    }

    string get_str_comment_depth(uint16_t max_comment_depth) {
        return string("['st_max_comment_depth', {'value':'") + std::to_string(max_comment_depth) + "'}]";
    }

    string get_str_social_acc(name social_acc) {
        return string("['st_social_acc', {'value':'") + name{social_acc}.to_string() + "'}]";
    }

    string get_str_referral_acc(name referral_acc) {
        return string("['st_referral_acc', {'value':'") + name{referral_acc}.to_string() + "'}]";
    }

    string get_str_curators_prcnt(uint16_t min_curators_prcnt, uint16_t max_curators_prcnt) {
        return string("['st_curators_prcnt', {'min_curators_prcnt':'") + std::to_string(min_curators_prcnt) + "','max_curators_prcnt':'" + std::to_string(max_curators_prcnt) + "'}]";
    }

    string get_str_bwprovider(account_name actor, permission_name permission) {
        return string("['st_bwprovider', {'actor':'") + name{actor}.to_string() + "','permission':'" + name{permission}.to_string() + "'}]";
    }

    string get_str_min_abs_rshares_param(uint64_t value) {
        return string("['st_min_abs_rshares', {'value':'") + std::to_string(value) + "'}]";
    }
    //// posting tables
    variant get_permlink(account_name acc, uint64_t id) {
        return _tester->get_chaindb_struct(_code, acc, N(permlink), id, "permlink");
    }

    variant get_message(uint64_t id) {
        return _tester->get_chaindb_struct(_code, _code, N(message), id, "message");
    }

    variant get_permlink(mssgid message_id) {
        variant obj = _tester->get_chaindb_lower_bound_struct(_code, message_id.author, N(permlink), N(byvalue),
                                                              message_id.get_unique_key(), "message");
        if (!obj.is_null() && obj.get_object().size()) {
            if(obj["value"].as<std::string>() == message_id.permlink) {
                return obj;
            }
        }
        return variant();
    }

    variant get_message(mssgid message_id) {
        auto permlink = get_permlink(message_id);
        if (!permlink.is_null() && permlink.get_object().size()) {
            return get_message(permlink["id"].as_uint64());
        }
        return variant();
    }

    // Note: vote scope is message author, not voter
    variant get_vote(account_name author, uint64_t id) {
        return _tester->get_chaindb_struct(_code, author, N(vote), id, "voteinfo");
    }

    std::vector<variant> get_reward_pools() {
        return _tester->get_all_chaindb_rows(_code, _code, N(rewardpools), false);
    }

    std::vector<variant> get_messages(account_name user) {
        auto raw_messages = _tester->get_all_chaindb_rows(_code, _code, N(message), false);
        auto raw_permlinks = _tester->get_all_chaindb_rows(_code, user, N(permlink), false);

        std::map<uint64_t, variant> src_permlinks_map;
        for (auto& perm: raw_permlinks) {
            src_permlinks_map[perm["id"].as_uint64()] = perm;
        }

        std::vector<variant> messages;
        messages.reserve(raw_messages.size());
        for (auto& src_mssg:  raw_messages) {
            auto src_perm = src_permlinks_map.find(src_mssg["id"].as_uint64());
            if (src_perm == src_permlinks_map.end()) {
                continue;
            }
            mvo dst_mssg(src_mssg.get_object());

            for (auto entry: src_perm->second.get_object()) {
                dst_mssg(entry.key(), entry.value());
            }
            dst_mssg("permlink", src_perm->second["value"]);
            messages.emplace_back(variant(dst_mssg));
        }

        return messages;
    }

    //// posting helpers
    mvo make_mvo_fn(const std::string& str, base_t max) {
        return mvo()("str", str)("maxarg", max);
    }
    mvo fn_to_mvo(const funcparams& fn) {
        return make_mvo_fn(fn.str, fn.maxarg);
    }

    const uint8_t max_vote_changes = 5;
    const uint32_t window = 120;
    const uint32_t upvote_lockout = 15;
    const uint8_t max_beneficiaries = 64;
    const uint16_t max_comment_depth = 127;
    const uint16_t min_curators_prcnt = 1000;
    const uint16_t max_curators_prcnt = 9000;
};


}} // eosio::testing
