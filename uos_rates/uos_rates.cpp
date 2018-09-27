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
#include <eosio/uos_rates/cvs.h>
//#include <eosio/producer_plugin/producer_plugin.hpp>


namespace eosio {
    using namespace std;

    static appbase::abstract_plugin& _uos_rates = app().register_plugin<uos_rates>();

    class uos_rates_impl {

        unordered_set<string> str_dictionary;;

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        std::vector<std::shared_ptr<singularity::relation_t>> parse_transactions_from_block(
                eosio::chain::signed_block_ptr block, uint32_t current_calc_block);

        void run_transaction(
                string account,
                string action,
                std::map<string, string> input_data,
                string pub_key,
                string priv_key);

        void set_rate(string name, string value);

        friend class uos_rates;

        CSVWriter logger{"result.csv"},logger_i{"input.csv"},error_log{"error.txt"};

    private:

        uint64_t last_calc_block = 0;
        const uint32_t period = 300*2;
        const uint32_t window = 86400*2*100;
        const string contract_acc = "uos.activity";
        const string init_priv_key = "5K2FaURJbVHNKcmJfjHYbbkrDXAt2uUMRccL6wsb2HX4nNU3rzV";
        const string init_pub_key = "EOS6ZXGf34JNpBeWo6TXrKFGQAJXTUwXTYAdnAN4cajMnLdJh2onU";

    };

    void uos_rates_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        auto latency = (fc::time_point::now() - bsp->block->timestamp).count()/1000;
        ilog(("latency " + std::to_string(latency)).c_str());



        if (latency > 100000)
            return;

        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        auto current_calc_block_num = irr_block_num - (irr_block_num % period);

        ilog((std::string("last_cal_block ") + std::to_string(last_calc_block) + " Current_cals_blocks "+std::to_string(current_calc_block_num)).c_str());
        if(last_calc_block >= current_calc_block_num)
            return;

        int32_t end_block = current_calc_block_num;
        int32_t start_block = end_block - window + 1;
        if (start_block < 1)
            start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));


        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        //activity calculator for social interactions
        singularity::parameters_t params;
        auto calculator =
                singularity::rank_calculator_factory::create_calculator_for_social_network(params);

        logger_i.is_write = true;
        logger_i.setApart(false);


        logger_i.setFilename(std::string("input_")+ fc::variant(fc::time_point::now()).as_string()+".csv");

        for(int i = start_block; i <= end_block; i++)
        {
            auto block = cc.fetch_block_by_number(i);

            auto interactions = parse_transactions_from_block(block, current_calc_block_num);

            calculator->add_block(interactions);
        }


        auto a_result = calculator->calculate();

        singularity::gravity_index_calculator grv_cals(0.1, 0.9, 100000000000);

        ilog("a_result.size()" + std::to_string(a_result.size()));

         logger.is_write = true;
         logger.setApart(false);


        for (auto group : a_result)
        {
            auto group_name = group.first;
            auto item_map = group.second;
            auto norm_map = grv_cals.scale_activity_index(*item_map);
            std::vector<std::string> vec;
            for (auto item : norm_map) {
                ilog(item.first + " " + item.second.str(5));

                //set_rate(item.first, std::to_string(item.second));
                std::map<string, string> input_data;
                input_data["name"] = item.first;

                fix_symbol(input_data["name"]);

                input_data["value"] = item.second.str(5);

                vec.reserve(input_data.size());
                std::for_each(input_data.begin(), input_data.end(),  [&](std::pair<const std::string, std::string>  & element){
                    vec.push_back(element.second);
                });
                vec.push_back("setrate");
                vec.push_back(fc::time_point::now());
                logger.addDatainRow(vec.begin(), vec.end());
                run_transaction(contract_acc, "setrate", input_data, init_pub_key, init_priv_key);
                vec.clear();
            }
            logger.setFilename("result_"+fc::variant(fc::time_point::now()).as_string()+"_"+ to_string_from_enum(group_name) +".csv");
        }

         last_calc_block = current_calc_block_num;
    }

    std::vector<std::shared_ptr<singularity::relation_t>> uos_rates_impl::parse_transactions_from_block(
            eosio::chain::signed_block_ptr block, uint32_t current_calc_block){

        std::vector<std::shared_ptr<singularity::relation_t>> interactions;
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        for (auto trs : block->transactions) {
            uint32_t block_height = current_calc_block - block->block_num();
            try {
                auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
                auto actions = transaction.actions;
                for (auto action : actions) {

                    if (action.account.to_string() != contract_acc)
                        continue;
                    if(action.name.to_string()!="usertouser" &&
                       action.name.to_string()!="makecontent" &&
                       action.name.to_string()!= "usertocont" &&
                       action.name.to_string()!= "makecontorg")
                        continue;

                    chain_apis::read_only::abi_bin_to_json_params bins;
                    bins.code = action.account;
                    bins.action = action.name;
                    bins.binargs = action.data;
                    auto json = ro_api.abi_bin_to_json(bins);
                    auto object = json.args.get_object();

                        std::string symbol="\n";

                    if (action.name.to_string() == "usertouser") {


//                        auto from = object["acc_from"].as_string();
//                        auto to = object["acc_to"].as_string();
//                        singularity::transaction_t tran(100000, 1, from, to, time_t(), 100000, 100000);
//                        transactions_t.push_back(tran);
//                        ilog("usertouser " + from + " " + to);
                    }


                    if (action.name.to_string() == "makecontent" ) {

                        auto from = object["acc"].as_string();
                        auto to = object["content_id"].as_string();
                        auto content_type_id = object["content_type_id"].as_string();
                        if(content_type_id == "5")
                            continue;

                        ownership_t ownership(from, to, block_height);
                        interactions.push_back(std::make_shared<ownership_t>(ownership));
                        ilog("makecontent " + from + " " + to);

                        std::string s1 = ownership.get_target();
                        fix_symbol(s1);
                        std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownership.get_source(),s1,
                                                     ownership.get_name(),std::to_string(ownership.get_height()),std::to_string(ownership.get_weight()),
                                                     std::to_string(ownership.get_reverse_weight()),to_string_from_enum(ownership.get_source_type()),to_string_from_enum(ownership.get_target_type())};
                        logger_i.addDatainRow(vec.begin(),vec.end());
                        vec.clear();


//                        auto parent = object["parent_content_id"].as_string();
//                        if(parent != "")
//                        {
//                            singularity::transaction_t tran2(100000, 1, from, parent, time_t(), 100000, 100000);
//                            transactions_t.push_back(tran2);
//                            ilog("parent content " + from + " " + parent);
//                        }
                    }

                    if (action.name.to_string() == "usertocont") {


                        auto from = object["acc"].as_string();
                        auto to = object["content_id"].as_string();
                        auto interaction_type_id = object["interaction_type_id"].as_string();
                        if(interaction_type_id == "2") {
                            upvote_t upvote(from, to, block_height);
                            interactions.push_back(std::make_shared<upvote_t>(upvote));
                            ilog("usertocont " + from + " " + to);

                            std::string s1 = upvote.get_target();
                            fix_symbol(s1);
                            std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),upvote.get_source(),s1, upvote.get_name(),std::to_string(upvote.get_height()),std::to_string(upvote.get_weight()),
                                                         std::to_string(upvote.get_reverse_weight()),to_string_from_enum(upvote.get_source_type()),to_string_from_enum(upvote.get_target_type())};
                            logger_i.addDatainRow(vec.begin(),vec.end());
                            vec.clear();
                        }
                        if(interaction_type_id == "4") {
                            downvote_t downvote(from, to, block_height);
                            interactions.push_back(std::make_shared<downvote_t>(downvote));
                            ilog("usertocont " + from + " " + to);

                            std::string s1 = downvote.get_target();
                            fix_symbol(s1);
                            std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),downvote.get_source(),s1,
                                                         downvote.get_name(),std::to_string(downvote.get_height()),std::to_string(downvote.get_weight()),
                                                         std::to_string(downvote.get_reverse_weight()),to_string_from_enum(downvote.get_source_type()),to_string_from_enum(downvote.get_target_type())};
                            logger_i.addDatainRow(vec.begin(),vec.end());
                            vec.clear();
                        }
                    }
                    if (action.name.to_string() == "makecontorg") {

                        auto from = object["organization_id"].as_string();
                        auto to = object["content_id"].as_string();
                        ownershiporg_t ownershiporg(from, to, block_height);
                        interactions.push_back(std::make_shared<ownershiporg_t>(ownershiporg));
                        ilog("makecontorg " + from + " " + to);

                        std::string s1 = ownershiporg.get_target();
                        fix_symbol(s1);
                        std::vector<std::string> vec{block->timestamp.to_time_point(),std::to_string(block->block_num()),ownershiporg.get_source(),s1,
                                                     ownershiporg.get_name(),std::to_string(ownershiporg.get_height()),std::to_string(ownershiporg.get_weight()),
                                                     std::to_string(ownershiporg.get_reverse_weight()),to_string_from_enum(ownershiporg.get_source_type()),to_string_from_enum(ownershiporg.get_target_type())};
                        logger_i.addDatainRow(vec.begin(),vec.end());
                        vec.clear();

                    }
                }
            }
            catch (...){
                ilog("exception" + std::to_string(block->block_num()));
                error_log.is_write = true;
                error_log.setApart(false);
                std::vector<std::string> err { std::to_string(block->block_num())};
                error_log.addDatainRow(err.begin(),err.end());
                err.clear();
            }
        }

        return interactions;
    }

    void uos_rates_impl::run_transaction(
            string account,
            string action,
            std::map<string, string> input_data,
            string pub_key,
            string priv_key)
    {
        auto creator_priv_key = fc::crypto::private_key(priv_key);
        auto creator_pub_key = fc::crypto::public_key(pub_key);
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

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(account);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = action;//!!!!!!!!!!!!!!! move constants to settings
        act.account = account;//!!!!!!!
        act.authorization = vector<chain::permission_level>{{account, chain::config::active_name}};//!!!!!!!!!!
        fc::mutable_variant_object data;
        for(auto item : input_data)
            data.set(item.first, item.second);

        act.data = eosio_token_serializer.variant_to_binary(action, data, fc::milliseconds(100));

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
            ilog("transaction sent " + action);

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
