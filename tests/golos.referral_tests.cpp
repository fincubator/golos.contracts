#include "golos_tester.hpp"
#include "golos.posting_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.referral_test_api.hpp"
#include "cyber.token_test_api.hpp"
#include "golos.vesting_test_api.hpp"
#include "golos.ctrl_test_api.hpp"
#include "contracts.hpp"


namespace cfg = golos::config;
using namespace eosio::testing;
using namespace eosio::chain;
using namespace fc;

class golos_referral_tester : public golos_tester {
public:
    golos_referral_tester()
        : golos_tester(cfg::referral_name)
        , _sym(3, "GLS")
        , _sym_vest(6, "GLS")
        , post({this, cfg::publish_name, _sym})
        , vest({this, cfg::vesting_name, _sym_vest})
        , referral({this, _code})
        , token({this, cfg::token_name, _sym})
        , ctrl({this, cfg::control_name})
        , _users{N(sania), N(pasha), N(tania), N(vania), N(issuer)}
    {
        create_accounts({N(sania), N(pasha), N(tania), N(vania), _code,
                         cfg::publish_name, cfg::token_name, cfg::issuer_name,
                         cfg::vesting_name, cfg::social_name, cfg::control_name,
                         cfg::charge_name});
        produce_blocks(2);

        install_contract(cfg::token_name, contracts::token_wasm(), contracts::token_abi());
        vest.add_changevest_auth_to_issuer(cfg::issuer_name, cfg::control_name);
        vest.initialize_contract(cfg::token_name);
        post.initialize_contract(cfg::token_name, cfg::charge_name);
        referral.initialize_contract(cfg::token_name);
        ctrl.initialize_contract(cfg::token_name);

        BOOST_CHECK_EQUAL(success(), token.create(cfg::issuer_name, token.make_asset(10000)));
        BOOST_CHECK_EQUAL(success(), token.open(cfg::publish_name, _sym, cfg::publish_name));

        produce_blocks();

        BOOST_CHECK_EQUAL(success(), vest.create_vesting(cfg::issuer_name, _sym_vest, cfg::control_name));
        for (auto& u : _users) {
            BOOST_CHECK_EQUAL(success(), vest.open(u));
        }
        produce_blocks();
    }

    void init_params() {
        auto breakout_parametrs = referral.breakout_parametrs(min_breakout, max_breakout);
        auto expire_parametrs   = referral.expire_parametrs(max_expire);
        auto percent_parametrs  = referral.percent_parametrs(max_percent);

        auto params = "[" + breakout_parametrs + "," + expire_parametrs + "," + percent_parametrs + "]";
        BOOST_CHECK_EQUAL(success(), referral.set_params(cfg::referral_name, params));
    }

    void init_params_posts() {
        funcparams fn{"0", 1};
        BOOST_CHECK_EQUAL(success(), post.set_rules(fn ,fn ,fn , 5000));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("comment"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("vote"));
        BOOST_CHECK_EQUAL(success(), post.set_limit("post bandwidth"));
        produce_blocks();

        auto vote_changes = post.get_str_vote_changes(post.max_vote_changes);
        auto cashout_window = post.get_str_cashout_window(post.window, post.upvote_lockout);
        auto beneficiaries = post.get_str_beneficiaries(post.max_beneficiaries);
        auto comment_depth = post.get_str_comment_depth(post.max_comment_depth);
        auto social_acc = post.get_str_social_acc(cfg::social_name);
        auto referral_acc = post.get_str_referral_acc(cfg::referral_name);
        auto curators_prcnt = post.get_str_curators_prcnt(post.min_curators_prcnt, post.max_curators_prcnt);
        auto bwprovider = post.get_str_bwprovider(name(), name());
        auto min_abs_rshares = post.get_str_min_abs_rshares_param(8);

        auto params = "[" + vote_changes + "," + cashout_window + "," + beneficiaries + "," + comment_depth +
                "," + social_acc + "," + referral_acc + "," + curators_prcnt + "," + bwprovider + "," + min_abs_rshares + "]";
        BOOST_CHECK_EQUAL(success(), post.set_params(params));
        produce_blocks();
    }

protected:
    symbol _sym;
    symbol _sym_vest;
    // TODO: make contract error messages more clear
    struct errors: contract_error_messages {
        const string referral_exist = amsg("A referral with the same name already exists");
        const string referral_equal = amsg("referral can not be referrer");
        const string min_expire     = amsg("expire <= current block time");
        const string max_expire     = amsg("expire > current block time + max_expire");
        const string min_breakout   = amsg("breakout < min_breakout");
        const string max_breakout   = amsg("breakout > max_breakout");
        const string persent        = amsg("specified parameter is greater than limit");

        const string min_more_than_max = amsg("min_breakout > max_breakout");
        const string negative_minimum  = amsg("min_breakout < 0");
        const string limit_percent     = amsg("max_percent > 100.00%");

        const string referral_not_exist      = amsg("A referral with this name doesn't exist.");
        const string funds_not_equal   = amsg("Amount of funds doesn't equal.");
        const string limit_percents    = amsg("weights_sum + referral percent must not be greater than 100% (10000)");
        const string referrer_benif    = amsg("Comment already has referrer as a referrer-beneficiary.");
    } err;

    golos_posting_api post;
    golos_vesting_api vest;
    golos_referral_api referral;
    cyber_token_api token;
    golos_ctrl_api ctrl;

    const asset min_breakout = token.make_asset(10);
    const asset max_breakout = token.make_asset(100);
    const uint64_t max_expire = 600; // 600 sec
    const uint16_t max_percent = 5000; // 50.00%

    std::vector<account_name> _users;
    };

BOOST_AUTO_TEST_SUITE(golos_referral_tests)

BOOST_FIXTURE_TEST_CASE(set_params, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test vesting parameters");
    BOOST_TEST_MESSAGE("--- prepare");
    produce_blocks();

    BOOST_TEST_MESSAGE("--- check that global params not exist");
    BOOST_TEST_CHECK(referral.get_params().is_null());

    init_params();
    produce_blocks();

    auto obj_params = referral.get_params();
    BOOST_TEST_MESSAGE("--- " + fc::json::to_string(obj_params));
    BOOST_TEST_CHECK(obj_params["breakout_params"]["min_breakout"].as<asset>() == min_breakout);
    BOOST_TEST_CHECK(obj_params["breakout_params"]["max_breakout"].as<asset>() == max_breakout);
    BOOST_TEST_CHECK(obj_params["expire_params"]["max_expire"].as<uint64_t>() == max_expire);
    BOOST_TEST_CHECK(obj_params["percent_params"]["max_percent"].as<uint16_t>() == max_percent);

    auto params = "[" + referral.breakout_parametrs(max_breakout, min_breakout) + "]";
    BOOST_CHECK_EQUAL(err.min_more_than_max, referral.set_params(cfg::referral_name, params));

    params = "[" + referral.breakout_parametrs(token.make_asset(-1), max_breakout) + "]";
    BOOST_CHECK_EQUAL(err.negative_minimum, referral.set_params(cfg::referral_name, params));

    params = "[" + referral.percent_parametrs(10001) + "]";
    BOOST_CHECK_EQUAL(err.limit_percent, referral.set_params(cfg::referral_name, params));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_referral_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test creating referral");

    init_params();
    produce_blocks();

    const auto current_time = control->head_block_time().sec_since_epoch();

    BOOST_TEST_MESSAGE("-- fail on bad params");
    BOOST_CHECK_EQUAL(err.referral_equal, referral.create_referral(N(issuer), N(issuer), 500, 0, token.make_asset(10)));

    BOOST_CHECK_EQUAL(err.min_expire, referral.create_referral(N(issuer), N(sania), 500, 0, token.make_asset(10)));
    BOOST_CHECK_EQUAL(err.max_expire, referral.create_referral(N(issuer), N(sania), 500, 999999999999, token.make_asset(10)));

    auto expire = 8; // sec
    BOOST_CHECK_EQUAL(err.min_breakout, referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(0)));
    BOOST_CHECK_EQUAL(err.max_breakout, referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(110)));

    BOOST_CHECK_EQUAL(err.persent, referral.create_referral(N(issuer), N(sania), 9500, current_time + expire, token.make_asset(50)));

    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(50)));

    BOOST_TEST_MESSAGE("-- fail on re-creating referral");
    produce_blocks();
    BOOST_CHECK_EQUAL(err.referral_exist, referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(50)));

    BOOST_TEST_MESSAGE("-- success with fixed breakout");
    auto params = "[" + referral.breakout_parametrs(max_breakout, max_breakout) + "]";
    BOOST_CHECK_EQUAL(success(), referral.set_params(cfg::referral_name, params));
    BOOST_CHECK_EQUAL(success(),
        referral.create_referral(N(issuer), N(pasha), max_percent, current_time + expire, max_breakout));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(transfer_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Transfer testing");

    init_params();
    const auto expire = 8;
    const auto breakout = 100;

    const auto current_time = control->head_block_time().sec_since_epoch();

    BOOST_TEST_MESSAGE("--- creating referral 'finteh.ref'");
    BOOST_CHECK(!referral.get_referral(cfg::referral_name));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), cfg::referral_name, 500, current_time + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(cfg::referral_name)["referral"].as<name>(), cfg::referral_name);

    BOOST_TEST_MESSAGE("--- creating referral 'vania'");
    BOOST_CHECK(!referral.get_referral(N(vania)));
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(vania), 500, current_time + expire, token.make_asset(breakout)));
    BOOST_CHECK_EQUAL(referral.get_referral(N(vania))["referral"].as<name>(), N(vania));

    BOOST_TEST_MESSAGE("--- issue tokens for users");
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::issuer_name, N(vania), token.make_asset(300), "issue 300 tokens for vania"));
    BOOST_CHECK_EQUAL(success(), token.issue(cfg::issuer_name, N(tania), token.make_asset(300), "issue 300 tokens for tania"));

    BOOST_TEST_MESSAGE("--- checking for asserts");
    BOOST_CHECK_EQUAL(err.referral_not_exist, token.transfer(N(tania), cfg::referral_name, token.make_asset(breakout), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout-1), ""));
    BOOST_CHECK_EQUAL(err.funds_not_equal, token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout+1), ""));

    BOOST_TEST_MESSAGE("--- checking that transfer was successful");
    BOOST_CHECK_EQUAL(success(), token.transfer(N(vania), cfg::referral_name, token.make_asset(breakout), ""));

    BOOST_TEST_MESSAGE("--- checking that record about referral was removed");
    BOOST_CHECK(!referral.get_referral(N(vania)));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(close_referral_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test close referral");

    init_params();

    const auto expire = 9; // sec
    const auto current_time = control->head_block_time().sec_since_epoch();
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(50)));
    BOOST_CHECK_EQUAL(success(), referral.close_old_referrals());
    produce_blocks();

    auto v_referrals = referral.get_referrals();
    BOOST_CHECK_EQUAL(v_referrals.size(), 1);
    BOOST_CHECK_EQUAL(v_referrals.at(0)["referral"].as<name>(), N(sania));
    BOOST_CHECK_EQUAL(v_referrals.at(0)["referrer"].as<name>(), N(issuer));

    produce_blocks(golos::seconds_to_blocks(expire));
    v_referrals = referral.get_referrals();
    BOOST_CHECK_EQUAL(v_referrals.size(), 1);
    BOOST_CHECK_EQUAL(success(), referral.close_old_referrals());
    v_referrals = referral.get_referrals();
    BOOST_CHECK_EQUAL(v_referrals.size(), 0);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(create_referral_message_tests, golos_referral_tester) try {
    BOOST_TEST_MESSAGE("Test creating message referral");
    init_params();
    init_params_posts();

    const auto expire = 8; // sec
    const auto current_time = control->head_block_time().sec_since_epoch();
    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(sania), 500, current_time + expire, token.make_asset(50)));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(sania), "permlink"}));

    auto post_sania = post.get_message({N(sania), "permlink"});
    BOOST_CHECK_EQUAL(post_sania["beneficiaries"].size(), 1);
    BOOST_CHECK_EQUAL(post_sania["beneficiaries"][uint8_t(0)].as<beneficiary>().account, N(issuer));

    BOOST_CHECK_EQUAL(success(), referral.create_referral(N(issuer), N(pasha), 5000, current_time + expire, token.make_asset(50)));
    BOOST_CHECK_EQUAL(err.limit_percents, post.create_msg({N(pasha), "permlink"}, {N(), "parentprmlnk"}, { beneficiary{N(tania), 7000} }));
    BOOST_CHECK_EQUAL(err.referrer_benif, post.create_msg({N(pasha), "permlink"}, {N(), "parentprmlnk"}, { beneficiary{N(issuer), 2000} }));
    BOOST_CHECK_EQUAL(success(), post.create_msg({N(pasha), "permlink"}, {N(), "parentprmlnk"}, { beneficiary{N(tania), 2000} }));

    auto post_pasha = post.get_message({N(pasha), "permlink"});
    BOOST_CHECK_EQUAL(post_pasha["beneficiaries"].size(), 2);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
