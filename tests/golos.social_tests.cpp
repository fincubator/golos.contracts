#define UNIT_TEST_ENV
#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.ctrl_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "golos.social_test_api.hpp"
#include <common/config.hpp>
#include "contracts.hpp"

using namespace golos;
using namespace eosio::chain;
using namespace eosio::testing;
namespace cfg = golos::config;

static const auto _token_name = "GOLOS";
static const auto _token_precision = 3;
static const auto _token_sym = symbol(_token_precision, _token_name);
static const auto _vesting_precision = 6;
static const auto _vesting_sym = symbol(_vesting_precision, _token_name);

class golos_social_tester : public golos_tester {
protected:
    golos_posting_api post;
    golos_vesting_api vest;
    cyber_token_api token;
    golos_social_api social;
    golos_ctrl_api ctrl;

    std::vector<account_name> _users;
public:

    golos_social_tester()
        : golos_tester(cfg::social_name)
        , post({this, cfg::publish_name, _token_sym})
        , vest({this, cfg::vesting_name, _vesting_sym})
        , token({this, cfg::token_name, _token_sym})
        , social({this, _code})
        , ctrl({this, cfg::control_name})
        , _users{"dave"_n, "erin"_n} {

        produce_block();
        create_accounts(_users);
        create_accounts({cfg::issuer_name, cfg::control_name, cfg::token_name, 
                cfg::vesting_name, cfg::social_name, cfg::publish_name, cfg::charge_name});
        produce_block();

        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        vest.add_changevest_auth_to_issuer(cfg::issuer_name, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        post.initialize_contract(cfg::token_name, cfg::charge_name);
        social.initialize_contract();
        ctrl.initialize_contract(cfg::token_name);

        produce_block();
    }

    void init(int64_t issuer_funds, int64_t user_vesting_funds) {
        BOOST_CHECK_EQUAL(success(), token.create(cfg::issuer_name, token.make_asset(issuer_funds)));
        BOOST_CHECK_EQUAL(success(), token.open(cfg::publish_name, _token_sym, cfg::publish_name));
        BOOST_CHECK_EQUAL(success(), vest.create_vesting(cfg::issuer_name));

        funcparams fn{"0", 1};
        BOOST_CHECK_EQUAL(success(), post.set_rules(fn, fn, fn, 5000));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("comment"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("vote"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post bandwidth"));

        BOOST_CHECK_EQUAL(success(), post.init_default_params());
        BOOST_CHECK_EQUAL(success(), post.set_params("["+post.get_str_social_acc(cfg::social_name)+"]"));

        produce_block();

        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u, _vesting_sym, u));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.issue(cfg::issuer_name, u, token.make_asset(user_vesting_funds), "add funds to erin"));
            produce_block();
            BOOST_CHECK_EQUAL(success(), token.transfer(u, cfg::vesting_name, token.make_asset(user_vesting_funds), "buy vesting for erin"));
            produce_block();
        }
    }

protected:

    struct errors: contract_error_messages {
        const string no_pinning                 = amsg("Pinning account doesn't exist.");
        const string cannot_pin_yourself        = amsg("You cannot pin yourself");
        const string cannot_unpin_yourself      = amsg("You cannot unpin yourself");
        const string no_blocking                = amsg("Blocking account doesn't exist.");
        const string cannot_block_yourself      = amsg("You cannot block yourself");
        const string cannot_unblock_yourself    = amsg("You cannot unblock yourself");
        const string cannot_pin_blocked         = amsg("You have blocked this account. Unblock it before pinning");
        const string cannot_unpin_not_pinned    = amsg("You have not pinned this account");
        const string already_pinned             = amsg("You already have pinned this account");
        const string cannot_unblock_not_blocked = amsg("You have not blocked this account");
        const string already_blocked            = amsg("You already have blocked this account");
        const string you_are_blocked            = amsg("You are blocked by this account");
        const string no_blocker                 = amsg("Blocker account doesn't exist.");
        const string blocker_block_himself      = amsg("Blocker cannot block himself");
        const string no_pinner                  = amsg("Pinner account doesn't exist.");
        const string pinner_pin_himself         = amsg("Pinner cannot pin himself");
        const string record_already_exists      = amsg("Record already exists.");
        string missing_auth(name arg) { return "missing authority of " + arg.to_string(); };
    } err;
};


BOOST_AUTO_TEST_SUITE(social_tests)

BOOST_FIXTURE_TEST_CASE(golos_pinning_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos pinning test");

    BOOST_TEST_MESSAGE("--- pin not-exist account");
    BOOST_CHECK_EQUAL(err.no_pinning, social.pin("dave"_n, "notexist"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin not-exist account");
    BOOST_CHECK_EQUAL(err.no_pinning, social.unpin("dave"_n, "notexist"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_pin_yourself, social.pin("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unpin_yourself, social.unpin("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin without record (success)");
    BOOST_CHECK_EQUAL(success(), social.unpin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin with wrong authority");
    BOOST_CHECK_EQUAL(err.missing_auth("dave"_n), social.push("pin"_n, "erin"_n, social.args()
        ("pinner", "dave"_n)
        ("pinning", "erin"_n)
    ));

    BOOST_TEST_MESSAGE("--- pin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.pin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_pinned, social.pin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin with wrong authority");
    BOOST_CHECK_EQUAL(err.missing_auth("dave"_n), social.push("unpin"_n, "erin"_n, social.args()
        ("pinner", "dave"_n)
        ("pinning", "erin"_n)
    ));

    BOOST_TEST_MESSAGE("--- unpin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unpin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin, and try unpin again (fail)");
    BOOST_CHECK_EQUAL(success(), social.block("dave"_n, "erin"_n));
    produce_block();
    BOOST_CHECK_EQUAL(err.cannot_unpin_not_pinned, social.unpin("dave"_n, "erin"_n));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_blocking_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos blocking test");

    BOOST_TEST_MESSAGE("--- block: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_block_yourself, social.block("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave dave");
    BOOST_CHECK_EQUAL(err.cannot_unblock_yourself, social.unblock("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin without record (success)");
    BOOST_CHECK_EQUAL(success(), social.unblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), social.block("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin again (fail)");
    BOOST_CHECK_EQUAL(err.already_blocked, social.block("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin dave erin, and try unblock again (fail)");
    BOOST_CHECK_EQUAL(success(), social.pin("dave"_n, "erin"_n));
    produce_block();
    BOOST_CHECK_EQUAL(err.cannot_unblock_not_blocked, social.unblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), social.block("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- pin: dave erin (fail)");
    BOOST_CHECK_EQUAL(err.cannot_pin_blocked, social.pin("dave"_n, "erin"_n));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_sync_pinning_blocking_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Testing pinning and blocking with _self authority");

    BOOST_TEST_MESSAGE("--- fail addpin: dave dave");
    BOOST_CHECK_EQUAL(err.pinner_pin_himself, social.addpin("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- addpin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.addpin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addblock: dave erin");
    BOOST_CHECK_EQUAL(err.record_already_exists, social.addblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unpin: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unpin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addblock: dave dave");
    BOOST_CHECK_EQUAL(err.blocker_block_himself, social.addblock("dave"_n, "dave"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- addblock: dave erin");
    BOOST_CHECK_EQUAL(success(), social.addblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addpin: dave erin");
    BOOST_CHECK_EQUAL(err.record_already_exists, social.addpin("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- unblock: dave erin");
    BOOST_CHECK_EQUAL(success(), social.unblock("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addpin: notexist erin");
    BOOST_CHECK_EQUAL(err.no_pinner, social.addpin("notexist"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addpin: dave notexist");
    BOOST_CHECK_EQUAL(err.no_pinning, social.addpin("dave"_n, "notexist"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addblock: notexist erin");
    BOOST_CHECK_EQUAL(err.no_blocker, social.addblock("notexist"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- fail addblock: dave notexist");
    BOOST_CHECK_EQUAL(err.no_blocking, social.addblock("dave"_n, "notexist"_n));
    produce_block();

} FC_LOG_AND_RETHROW();

BOOST_FIXTURE_TEST_CASE(golos_blocked_commenting_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Test commenting when blocked");

    auto bignum = 1000000;
    init(bignum, 10);

    BOOST_TEST_MESSAGE("--- create post: dave");
    BOOST_CHECK_EQUAL(success(), post.create_msg({"dave"_n, "permlink"}));
    produce_block();

    BOOST_TEST_MESSAGE("--- create comment: erin to dave");
    BOOST_CHECK_EQUAL(success(), post.create_msg({"erin"_n, "permlink2"},
                                                 {"dave"_n, "permlink"}));
    produce_block();

    BOOST_TEST_MESSAGE("--- block: dave erin");
    BOOST_CHECK_EQUAL(success(), social.block("dave"_n, "erin"_n));
    produce_block();

    BOOST_TEST_MESSAGE("--- create comment: erin to dave again (fail)");
    BOOST_CHECK_EQUAL(err.you_are_blocked, post.create_msg({"erin"_n, "permlink3"},
                                                           {"dave"_n, "permlink"}));
//    BOOST_TEST_MESSAGE("--- create comment: erin to dave again (success)");
//    BOOST_CHECK_EQUAL(success(), post.create_msg({"erin"_n, "permlink3"},
//                                                           {"dave"_n, "permlink"}));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(golos_accountmeta_test, golos_social_tester) try {
    BOOST_TEST_MESSAGE("Simple golos accountmeta test");

    accountmeta meta;

    BOOST_TEST_MESSAGE("--- hack attempt: erin wants to update dave's meta");
    meta.first_name = "Fool";
    auto hack_attempt = social.push("updatemeta"_n, "erin"_n, social.args()
        ("account", "dave"_n)
        ("meta", meta)
    );
    BOOST_CHECK_NE(success(), hack_attempt);
    produce_block();

    BOOST_TEST_MESSAGE("--- updatemeta: dave");
    meta.first_name = "Dave";
    BOOST_CHECK_EQUAL(success(), social.updatemeta("dave"_n, meta));
    produce_block();

    BOOST_TEST_MESSAGE("--- hack attempt: erin wants to delete dave's meta");
    hack_attempt = social.push("deletemeta"_n, "erin"_n, social.args()
        ("account", "dave"_n)
    );
    BOOST_CHECK_NE(success(), hack_attempt);
    produce_block();

    BOOST_TEST_MESSAGE("--- deletemeta: dave");
    BOOST_CHECK_EQUAL(success(), social.deletemeta("dave"_n));
    produce_block();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
