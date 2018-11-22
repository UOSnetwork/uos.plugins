#include <eosio/u_com/u_com.hpp>
#include <eosio/u_com/transaction_queqe.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <amqpcpp.h>
#include "SimplePocoHandler.h"


#include <fc/io/json.hpp>
#include <algorithm>
#include <boost/program_options.hpp>
#include <regex>
#include <iostream>


namespace eosio {
    using namespace std;


    bool is_valid_regex_string(const std::string& rgx_str)
    {
        bool bResult = true;
        try
        {
            std::regex tmp(rgx_str);
        }
        catch (const std::regex_error& e)
        {
            (e);
            bResult = false;
        }
        return bResult;
    }

    static appbase::abstract_plugin& _u_com = app().register_plugin<u_com>();

    class u_com_impl {

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);

        void  first_run_plugin();
        void  parse_transactions_from_block(eosio::chain::signed_block_ptr block);


        void run_trx_queue(uint64_t num);

        vector<account_name> get_all_accounts();

        void run_transaction(
                string account,
                string action,
                fc::mutable_variant_object data,
                string pub_key,
                string priv_key,
                string acc_from = "");

        void run_transaction(transaction_queue &temp);

        void add_transaction(
                string account,
                string action,
                fc::mutable_variant_object data,
                string pub_key,
                string priv_key,
                string acc_from = "");

        void run_transaction(trx_to_run trx){run_transaction(trx.account,trx.action,trx.data,trx.pub_key,trx.priv_key,trx.acc_from);}

        void add_transaction(trx_to_run trx){trx_queue.push(trx);}

        boost::program_options::variables_map _options;

        friend class u_com;

//        CSVWriter social_activity_log,transfer_activity_log;

    private:

        int32_t period = 300*2;
//        int32_t window = 86400*2*100;
        string contract_activity = "uos.activity";
        string contract_calculators = "uos.calcs";
        std::set<chain::account_name> calculators;
        string calculator_public_key = "";
        string calculator_private_key = "";
        string calc_contract_public_key = "";
        string calc_contract_private_key = "";
        bool is_first_parse =  false;

//        double social_importance_share = 0.1;
//        double transfer_importance_share = 0.1;
//        double stake_importance_share = 1.0 - social_importance_share - transfer_importance_share;

//        const uint32_t seconds_per_year = 365*24*3600;
//        const double yearly_emission_percent = 1.0;
//        const int64_t initial_token_supply = 1000000000;
//        const uint8_t blocks_per_second = 2;
//        uint64_t last_calc_block = 0;

//        bool dump_calc_data = false;
//        bfs::path dump_dir;

//        uint64_t last_setrate_block = 0;

        transaction_queue trx_queue;
    };

    void u_com_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {

        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        //determine current calculating block number
        auto irr_block_id = bsp->block->id();
        auto irr_block_num = bsp->block->num_from_id(irr_block_id);
        auto timestamp =  bsp->block->timestamp.to_time_point();
//        elog("Catch irrreversible blocks" + to_string(irr_block_num));

        for (auto trs : bsp->block->transactions) {

            auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
            auto actions = transaction.actions;
            auto transaction_id = transaction.id();
            for (auto action : actions) {
                if(action.account == N(eosio.token) || action.account == N(uos.activity) || action.account == N(uos.calcs)){
                    fc::mutable_variant_object act;
                    act["block_num"]=fc::variant(irr_block_num);
                    act["block_timestamp"]=fc::variant(timestamp);
                    act["transaction_id"]=fc::variant(transaction_id);
                    act["account"]=fc::variant(action.account);
                    act["action"]=fc::variant(action.name);
//                    act["receiver"] = fc::variant(action.authorization);
                    act["data"] = ro_api.abi_bin_to_json({action.account,action.name,action.data}).args;

                    //TODO:rename queue,param
                    SimplePocoHandler handler("localhost", 5672);
                    AMQP::Connection connection(&handler, AMQP::Login("guest", "guest"), "/");
                    AMQP::Channel channel(&connection);

                    channel.onReady([&](){
                        if(handler.connected()){
                            channel.publish("", "hello", fc::json::to_string(act));
                            handler.quit();
                        }
                    });
                    handler.loop();

                }
            }
        }
    }

    void u_com_impl::first_run_plugin()
    {
        if(is_first_parse == true)
            return;

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        int32_t start_block = 0;
        auto current_head_block_number = cc.fork_db_head_block_num();
        elog("Current blocks number" + to_string(current_head_block_number));
        int32_t end_block =  current_head_block_number;
        if (start_block < 1)
            start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));
        for (int i = start_block; i <= end_block; i++) {
            try {
                auto block = cc.fetch_block_by_number(i);
                elog("Current parse block:" + to_string(i));
                parse_transactions_from_block(block);
//                if (i == 1600000)
//                {
//                is_first_parse = true;
//                return;
//                }
            }

            catch (...) {
                elog("Error on parsing block " + std::to_string(i));
            }
        }

        is_first_parse = true;
    }

    void u_com_impl::parse_transactions_from_block(eosio::chain::signed_block_ptr block)
    {

    auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        auto current_block_id = block->id();
        auto current_block_num = block->num_from_id(current_block_id);
    for (auto trs : block->transactions) {
        auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
        auto actions = transaction.actions;
            auto transaction_id = transaction.id();
            for (auto action : actions) {
                if(action.account == N(eosio.token) || action.account == N(uos.activity) || action.account == N(uos.calcs)){
                    fc::mutable_variant_object act;
                    act["block_num"]=fc::variant(current_block_num);
//                    act["block_timestamp"]=fc::variant(timestamp);
                    act["transaction_id"]=fc::variant(transaction_id);
                    act["account"]=fc::variant(action.account);
                    act["action"]=fc::variant(action.name);
                    act["data"] = ro_api.abi_bin_to_json({action.account,action.name,action.data}).args;

                    //TODO:rename queue,param
                    SimplePocoHandler handler("localhost", 5672);
                    AMQP::Connection connection(&handler, AMQP::Login("guest", "guest"), "/");
                    AMQP::Channel channel(&connection);

                    channel.onReady([&](){
                        if(handler.connected()){
                            channel.publish("", "hello", fc::json::to_string(act));
                            handler.quit();
                        }
                    });
                    handler.loop();

                }
            }
        }
}


    vector<account_name > u_com_impl::get_all_accounts()
    {
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        const auto &database = cc.db();
        typedef typename chainbase::get_index_type< chain::account_object >::type index_type;
        const auto &table = database.get_index<index_type,chain::by_name>();
        std::vector <account_name > account_name;
        for(chain::account_object item: table){
           // ilog(c_fail + "ACCOUNTS:" + item.name.to_string()+ c_clear);
            account_name.push_back(item.name);
        }
        return account_name;
    }

    void u_com_impl::run_trx_queue(uint64_t num) {
        if (num == 0)
            return;
        if((fc::time_point::now() - app().get_plugin<chain_plugin>().chain().head_block_header().timestamp)  < fc::seconds(3)) {
            ilog("Run transactions");
            if (trx_queue.empty())
                return;
            trx_to_run last = trx_queue.front();
            transaction_queue temp;
            trx_queue.pop();
            temp.push(last);
            for (uint64_t i = 1; i < num; i++) {
                if (trx_queue.empty())
                    break;
                if((last.account==trx_queue.front().account)&&(last.action==trx_queue.front().action)){
                    temp.push(trx_queue.front());
                    trx_queue.pop();
                }
                else{
                    run_transaction(temp);
                    if(temp.size()>0){
                        elog("queue > 0, something went wrong");
                        return;
                    }
                    last = trx_queue.front();
                    temp.push(last);
                    trx_queue.pop();
                }
            }
            run_transaction(temp);
            if(temp.size()>0){
                elog("queue > 0, something went wrong");
                return;
            }
        }
    }

    void u_com_impl::add_transaction(
            string account,
            string action,
            fc::mutable_variant_object data,
            string pub_key,
            string priv_key,
            string acc_from)
    {
        trx_queue.emplace(trx_to_run(account,action,data,pub_key,priv_key,acc_from));
    }

    void u_com_impl::run_transaction(transaction_queue &temp){
        auto creator_priv_key = fc::crypto::private_key(temp.front().priv_key);
        auto creator_pub_key = fc::crypto::public_key(temp.front().pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(temp.front().account);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = temp.front().action;//!!!!!!!!!!!!!!! move constants to settings
        act.account = temp.front().account;//!!!!!!!
        act.authorization = vector<chain::permission_level>{{temp.front().acc_from, chain::config::active_name}};

        while(temp.size()) {
            act.data = eosio_token_serializer.variant_to_binary(temp.front().action, temp.front().data, fc::milliseconds(100));
            signed_trx.actions.push_back(act);
            temp.pop();
        }
        signed_trx.expiration = cc.head_block_time() + fc::seconds(5);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());
        try {
            app().get_method<eosio::chain::plugin_interface::incoming::methods::transaction_async>()(
                    std::make_shared<chain::packed_transaction>(chain::packed_transaction(move(signed_trx))),
                    true,
                    [this](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr>& result) -> void{
                        if (result.contains<fc::exception_ptr>()) {
                            elog(fc::json::to_string(result.get<fc::exception_ptr>()));
                        } else {
                            auto trx_trace_ptr = result.get<chain::transaction_trace_ptr>();

                            try {
                                fc::variant pretty_output;
                                pretty_output = app().get_plugin<chain_plugin>().chain().to_variant_with_abi(*trx_trace_ptr, fc::milliseconds(100));
                                ilog(fc::json::to_string(pretty_output));
                            }
                            catch (...){
                                elog("Error ");
                            }
                        }
                    });
            ilog("transaction sent ");

        } catch (...) {
            elog("Error in accept transaction");
        }

    }

    void u_com_impl::run_transaction(
            string account,
            string action,
            fc::mutable_variant_object data,
            string pub_key,
            string priv_key,
            string acc_from)
    {
        auto creator_priv_key = fc::crypto::private_key(priv_key);
        auto creator_pub_key = fc::crypto::public_key(pub_key);
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        chain::signed_transaction signed_trx;
        chain::action act;
        chain::abi_serializer eosio_token_serializer;

        auto &accnt = cc.db().get<chain::account_object, chain::by_name>(account);
        eosio_token_serializer.set_abi(accnt.get_abi(), fc::milliseconds(100));

        act.name = action;//!!!!!!!!!!!!!!! move constants to settings
        act.account = account;//!!!!!!!
        act.authorization = vector<chain::permission_level>{{acc_from, chain::config::active_name}};

        act.data = eosio_token_serializer.variant_to_binary(action, data, fc::milliseconds(100));

        //signed_trx.actions.emplace_back(act);
        signed_trx.actions.push_back(act);

        signed_trx.expiration = cc.head_block_time() + fc::seconds(5);
        signed_trx.set_reference_block(cc.head_block_id());
        signed_trx.max_net_usage_words = 5000;
        signed_trx.sign(creator_priv_key, cc.get_chain_id());

        try {

            app().get_method<eosio::chain::plugin_interface::incoming::methods::transaction_async>()(
                    std::make_shared<chain::packed_transaction>(chain::packed_transaction(move(signed_trx))),
                    true,
                    [this](const fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr>& result) -> void{
                    if (result.contains<fc::exception_ptr>()) {
                        elog(fc::json::to_string(result.get<fc::exception_ptr>()));
                    } else {
                        auto trx_trace_ptr = result.get<chain::transaction_trace_ptr>();

                        try {
                            fc::variant pretty_output;
                            pretty_output = app().get_plugin<chain_plugin>().chain().to_variant_with_abi(*trx_trace_ptr, fc::milliseconds(100));
                            ilog(fc::json::to_string(pretty_output));
                        }
                        catch (...){
                            elog("Error ");
                        }
                    }
            });

            ilog("transaction sent " + action);

        } catch (...) {
            elog("Error in accept transaction");
        }
    }

    u_com::u_com():my(new u_com_impl()){}
    u_com::~u_com(){}

    void u_com::set_program_options(options_description&, options_description& cfg) {
    }

    void u_com::plugin_initialize(const variables_map& options) {

    }

    void u_com::plugin_startup() {

        ilog( "starting u_com" );
        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        cc.irreversible_block.connect([this](const auto& bsp){
            my->irreversible_block_catcher(bsp);
        });

        my->first_run_plugin();
    }

    void u_com::plugin_shutdown() {
        // OK, that's enough magic
    }

    void u_com::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}
    void u_com::first_run_plugin() { my->first_run_plugin();}

}
