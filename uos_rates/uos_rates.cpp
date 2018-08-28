#include <eosio/uos_rates/uos_rates.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <../../../libraries/singularity/include/singularity.hpp>

#include <fc/io/json.hpp>

#include <unordered_set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>


namespace eosio {
    using namespace std;

    static appbase::abstract_plugin& _uos_rates = app().register_plugin<uos_rates>();

    class uos_rates_impl {

        unordered_set<string> str_dictionary;;

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        std::vector<singularity::transaction_t> parse_transactions_from_block(
                eosio::chain::signed_block_ptr block);

        void set_rate(string name, string value);

        friend class uos_rates;

    private:

        uint64_t last_calc_block = 0;
        const uint32_t period = 30*2;
        const uint32_t window = 86400*2*100;
        const string contract_acc = "uos.activity";
        const string init_priv_key = "5K2FaURJbVHNKcmJfjHYbbkrDXAt2uUMRccL6wsb2HX4nNU3rzV";
        const string init_pub_key = "EOS6ZXGf34JNpBeWo6TXrKFGQAJXTUwXTYAdnAN4cajMnLdJh2onU";
    };

    void uos_rates_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        auto latency = (fc::time_point::now() - bsp->block->timestamp).count()/1000;


        if (latency > 10000)
            return;

        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        auto current_calc_block_num = irr_block_num - (irr_block_num % period);

        if(last_calc_block >= current_calc_block_num)
            return;

        int32_t end_block = current_calc_block_num;
        int32_t start_block = end_block - window + 1;
        if (start_block < 1)
            start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));


        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        //activity calculator from singularity
        singularity::parameters_t params;
        singularity::activity_index_calculator a_calc(params);

        for(int i = start_block; i <= end_block; i++)
        {
            auto block = cc.fetch_block_by_number(i);

            auto transactions_t = parse_transactions_from_block(block);

            a_calc.add_block(transactions_t);
        }

        auto a_result = a_calc.calculate();
        ilog("a_result.size()" + std::to_string(a_result.size()));

        for (auto item : a_result)
        {
            ilog(item.first + " " + std::to_string(item.second));
            set_rate(item.first, std::to_string(item.second));
        }
        last_calc_block = current_calc_block_num;
    }

    std::vector<singularity::transaction_t> uos_rates_impl::parse_transactions_from_block(
            eosio::chain::signed_block_ptr block){
        std::vector<singularity::transaction_t> transactions_t;
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        for (auto trs : block->transactions) {
            try {
                auto actions = trs.trx.get<chain::packed_transaction>().get_transaction().actions;
                for (auto action : actions) {

                    if (action.account.to_string() != contract_acc)
                        continue;

                    chain_apis::read_only::abi_bin_to_json_params bins;
                    bins.code = action.account;
                    bins.action = action.name;
                    bins.binargs = action.data;
                    auto json = ro_api.abi_bin_to_json(bins);
                    auto object = json.args.get_object();



                    if (action.name.to_string() == "usertouser") {

                        auto from = object["acc_from"].as_string();
                        auto to = object["acc_to"].as_string();
                        singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
                        transactions_t.push_back(tran);
                        ilog("usertouser " + from + " " + to);
                    }

                    if (action.name.to_string() == "makecontent") {

                        auto from = object["content_id"].as_string();
                        auto to = object["acc"].as_string();
                        singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
                        transactions_t.push_back(tran);
                        ilog("makecontent " + from + " " + to);

                        auto parent = object["parent_content_id"].as_string();
                        if(parent != "")
                        {
                            singularity::transaction_t tran2(100000, 1, from, parent, time_t(), 100000, 100000);
                            transactions_t.push_back(tran2);
                            ilog("parent content " + from + " " + parent);
                        }
                    }

                    if (action.name.to_string() == "usertocont") {

                        auto from = object["acc"].as_string();
                        auto to = object["content_id"].as_string();
                        singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
                        transactions_t.push_back(tran);
                        ilog("usertocont " + from + " " + to);
                    }
                }
            }
            catch (...){
                ilog("exception");
            }
        }

        return transactions_t;
    }

    void uos_rates_impl::set_rate(std::string name, std::string value) {

        auto creator_priv_key = fc::crypto::private_key(init_priv_key);
        auto creator_pub_key = fc::crypto::public_key(init_pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        if(cc.pending_block_state()== nullptr){
            ilog("catch nullptr in activity");
        }
        else{
            ilog(fc::string(cc.pending_block_state()->header.timestamp.to_time_point()));
        }

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(contract_acc);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = N(setrate);//!!!!!!!!!!!!!!! move constants to settings
        act.account = N(uos.activity);//!!!!!!!
        act.authorization = vector<chain::permission_level>{{N(uos.activity), chain::config::active_name}};//!!!!!!!!!!
        fc::mutable_variant_object data;
        data.set("name", name);
        data.set("value", value);
        act.data = eosio_token_serializer.variant_to_binary("setrate", data, fc::milliseconds(100));

        //signed_trx.actions.emplace_back(act);
        signed_trx.actions.push_back(act);

        signed_trx.expiration = cc.head_block_time() + fc::seconds(500);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());

        try {
            app().get_plugin<chain_plugin>().accept_transaction(
                    chain::packed_transaction(move(signed_trx)),
                    [](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr> &result) {});
            ilog("transaction sent " + value);

        } catch (...) {
            elog("Error in accept transaction");
        }

    }

    uos_rates::uos_rates():my(new uos_rates_impl()){}
    uos_rates::~uos_rates(){}

    void uos_rates::set_program_options(options_description&, options_description& cfg) {
        cfg.add_options()
                ("gr_catch_email", bpo::value<string>()->default_value("yes"),
                 "Enable email catcher")
                ;
        cfg.add_options()
                ("gr_catch_string", bpo::value<vector<string>>()->composing(),
                 "Catch specific string")
                ;
    }

    void uos_rates::plugin_initialize(const variables_map& options) {
        if(options.count("gr_catch_string")) {
            auto strings = options["gr_catch_string"].as<vector<string>>();
            for( auto item : strings){
                my->str_dictionary.insert(item);
            }
        }
    }

    void uos_rates::plugin_startup() {
        ilog( "starting uos_rates" );

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        cc.irreversible_block.connect([this](const auto& bsp){my->irreversible_block_catcher(bsp);});

    }

    void uos_rates::plugin_shutdown() {
        // OK, that's enough magic
    }

    void uos_rates::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}

}
