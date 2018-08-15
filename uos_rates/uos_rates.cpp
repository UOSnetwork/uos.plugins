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
    void set_property_transaction(std::string account_name,
                                  std::string property_name,
                                  std::string property_value);
    class uos_rates_impl {

        unordered_set<string> str_dictionary;;

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        std::vector<singularity::transaction_t> parse_transactions_from_block(
                eosio::chain::signed_block_ptr block);

        void set_property_transaction(string account_name, string property_name, string property_value);
        string main_report(string);

        friend class uos_rates;

    private:

        uint64_t last_calc_block = 0;
        const uint32_t period = 30*2;
        const uint32_t window = 86400*2*100;

        std::map<string, string> last_results;
        bool results_ready = false;
    };

    string uos_rates_impl::main_report(std::string body) {
        if(!uos_rates_impl::results_ready)
            return R"({"msg":"Nothing to save"})";

        ilog("last_results.count() " + std::to_string(last_results.size()));
        for(auto item : last_results){
            uos_rates_impl::set_property_transaction(item.first, "rate", item.second);
        }
        for (auto item1 : last_results)
        {
            ilog("last_results "+ item1.first + " " + item1.second);
        }
        results_ready = false;
        return R"({"msg":"Results saved"})";
    }



    void uos_rates_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        auto latency = (fc::time_point::now() - bsp->block->timestamp).count()/1000;
        ilog("latency " + std::to_string(latency));

        if (latency > 10000)
            return;

        ilog("irreversible_block_catcher started");
        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        ilog("irr_block_num " + std::to_string(irr_block_num));

        auto current_calc_block_num = irr_block_num - (irr_block_num % period);
        ilog("current_calc_block_num " + std::to_string(current_calc_block_num));

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

        last_results.clear();
        for (auto item : a_result)
        {
            ilog(item.first + " " + std::to_string(item.second));
            last_results[item.first] = std::to_string(item.second);
        }
        for (auto item1 : last_results)
        {
            ilog("last_results "+ item1.first + " " + item1.second);
        }
        last_calc_block = current_calc_block_num;
        results_ready = true;
    }

    std::vector<singularity::transaction_t> uos_rates_impl::parse_transactions_from_block(
            eosio::chain::signed_block_ptr block){
        std::vector<singularity::transaction_t> transactions_t;
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        for (auto trs : block->transactions) {
            try {
                auto actions = trs.trx.get<chain::packed_transaction>().get_transaction().actions;
                for (auto action : actions) {

                    chain_apis::read_only::abi_bin_to_json_params bins;
                    bins.code = action.account;
                    bins.action = action.name;
                    bins.binargs = action.data;
                    auto json = ro_api.abi_bin_to_json(bins);
                    auto object = json.args.get_object();

                    if (action.name.to_string() == "add")
                    {
                        ilog("block " + std::to_string(block->block_num()) +
                             " account " + action.account.to_string() +
                             " action " + action.name.to_string());
                        ilog("json " + fc::json::to_string(json.args));
                        ilog(object["from_acc"].as_string());
                        ilog(object["to_acc"].as_string());
                        ilog(object["iweight"].as_string());
                    }

                    if (action.account.to_string() != "grv.likes")
                        continue;

                    if (action.name.to_string() != "add")
                        continue;

                    auto from = object["from_acc"].as_string();
                    auto to = object["to_acc"].as_string();
                    auto weight = object["iweight"].as_string();

                    singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
                    transactions_t.push_back(tran);

                    ilog("block " + std::to_string(block->block_num()) +
                         " account " + action.account.to_string() +
                         " action " + action.name.to_string());
                    ilog("from " + from +
                         " to " + to +
                         " iweight " + weight);
                    ilog("json " + fc::json::to_string(json.args));
                }
            }
            catch (...){
                ilog("exception");
            }
        }

        return transactions_t;
    }

    void uos_rates_impl::set_property_transaction(std::string account_name,
                                                     std::string property_name,
                                                     std::string property_value) {
        ilog("set_property_transaction " + account_name + " " + property_name + " " + property_value);

        string init_priv_key = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";
        string init_pub_key = "EOS6df7gX1Q2Qj2y2NAMpQt7rTtxrReeP4MrDmXJJjg8wpctrsPrd";

        auto creator_priv_key = fc::crypto::private_key(init_priv_key);
        auto creator_pub_key = fc::crypto::public_key(init_pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        if(cc.pending_block_state()== nullptr){
            elog("catch nullptr in activity");
        }
        else{
            ilog(fc::string(cc.pending_block_state()->header.timestamp.to_time_point()));
        }

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(N(grv.users));
        eosio_token_serializer.set_abi(accnt.get_abi());

        act.name = N(edtparam);
        act.account = N(grv.users);
        act.authorization = vector<chain::permission_level>{{N(eosio), chain::config::active_name}};
        fc::mutable_variant_object data;
        data.set("acc", account_name);
        data.set("param_name", property_name);
        data.set("param_value", property_value);
        act.data = eosio_token_serializer.variant_to_binary("edtparam", data);

        //signed_trx.actions.emplace_back(act);
        signed_trx.actions.push_back(act);

        signed_trx.expiration = cc.head_block_time() + fc::seconds(500);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());

        ilog("almost did it");
        try {
            app().get_plugin<chain_plugin>().accept_transaction(
                    chain::packed_transaction(move(signed_trx)),
                    [](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr> &result) {
                        ilog("started reporting result");
//                    if (result.contains<fc::exception_ptr>()) {
//                        auto e_ptr = result.get<fc::exception_ptr>();
//                        if (e_ptr->code() != chain::tx_duplicate::code_value && e_ptr->code() != chain::expired_tx_exception::code_value)
//                            elog("accept txn threw  ${m}",("m",result.get<fc::exception_ptr>()->to_detail_string()));
////                        elog(c, "bad packed_transaction : ${m}", ("m",result.get<fc::exception_ptr>()->what()));
//                    } else {
//                        auto trace = result.get<chain::transaction_trace_ptr>();
//                        if (!trace->except) {
//                            elog("chain accepted transaction");
//                            return;
//                        }
//
//                        //elog(c, "bad packed_transaction : ${m}", ("m",trace->except->what()));
//                    }
                    });
            ilog("transaction sent " + property_value);

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

        app().get_plugin<http_plugin>().
                add_handler(
                "/v1/uos_rates",
                [this](string url,string body,url_response_callback cb)mutable{
                    auto result=my->main_report(body);
                    cb(200, result);
                }
        );

    }

    void uos_rates::plugin_shutdown() {
        // OK, that's enough magic
    }

    void uos_rates::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}

}
