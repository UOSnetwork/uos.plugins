#include <eosio/u_com/u_com.hpp>
#include <eosio/u_com/transaction_queqe.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <amqpcpp.h>
#include "SimplePocoHandler.h"
#include "thread_safe.hpp"
#include "rabbitmq_worker.hpp"
#include  <mutex>


#include <fc/io/json.hpp>
#include <algorithm>
#include <boost/program_options.hpp>
#include <regex>
#include <iostream>

namespace eosio {
    using namespace std;
    using RB_client = function<void(fc::mutable_variant_object &data)>;

    class u_com_impl;
    enum DRR_Tags
    {
        DRR_TAG_OWNERNAME = 1,
        DRR_TAG_ACTIONINFO,//action
        DRR_TAG_IRRBLOCK,//irreversible blocks
        DRR_TAG_ALLBLOCK,//all blocks to current number
        DRR_TAG_ACCBLOCK,//accepted blocks
        DRR_TAG_LAST = 1001
    };

    const char *s_DRR_Tags[] = {"ownername", "actioninfo", "irrblock", "allblock", "accblock","datainfo","error","initial_port"};


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


    void rb_chanel_data(fc::mutable_variant_object data) {
        SimplePocoHandler handler("localhost", 5672);
        AMQP::Connection connection(&handler, AMQP::Login("guest", "guest"), "/");
        AMQP::Channel channel(&connection);

        channel.onReady([&]() {
            if (handler.connected()) {
                channel.publish("", "hello", fc::json::to_string(data));
//                elog("DATA:" +fc::json::to_string(data) );
                handler.quit();
            } else {
                elog("Handler not connected");
            }
        });
        handler.loop();
    };


    static appbase::abstract_plugin& _u_com = app().register_plugin<u_com>();

    class u_com_impl {
         thread *th,*th_data;
         RB_client rb_res = rb_chanel_data;
         shared_ptr<thread_safe::threadsafe_queue<string>> q_from_rabbit;
         std::recursive_mutex lock_rb_chanel;

    public:

        void irreversible_block_catcher(const chain::block_state_ptr& bsp);
        void accepted_block_catcher(const eosio::chain::block_state_ptr& asp);
        void parse_blocks();
        void parse_transactions_from_block(const eosio::chain::signed_block_ptr &block, fc::mutable_variant_object &tags);

        void userres(const uint current_head_block_number,string block_id);
        void rpc_server();
        void rpc_listen();
        void runMultiThread();
        void rb_chanel(fc::mutable_variant_object data);

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
        bool is_parse_blocks = false;
        string queue_host = "localhost";
        string queue_name = "hello";
        string queue_rpc = "rpc_queue";
        uint32_t queue_port = 5672;
        string queue_login = "guest";
        string queue_passwd = "guest";
        uint32_t start_block = 1;
        uint32_t end_block_parse = 1;




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

        fc::mutable_variant_object act;
        act["action"] = s_DRR_Tags[DRR_TAG_ACTIONINFO];
        act["type"] =s_DRR_Tags[DRR_TAG_IRRBLOCK];
        parse_transactions_from_block(bsp->block,act);

    }

    void u_com_impl::accepted_block_catcher(const eosio::chain::block_state_ptr &asp)
    {

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();
        chain_apis::read_only::get_block_params t;
        auto current_head_block_number = cc.fork_db_head_block_num();
        auto current_head_id_block = fc::variant(cc.fork_db_head_block_id()).as_string();
        //TODO: or period or each accepted  block
//        if(current_head_block_number % period == 0) { // period  equal blocks
        if(current_head_block_number % 100 == 0) {
            //            ilog("Staked balances snapshot for block " + to_string(current_head_block_number)+" ID :" + current_head_id_block);
            elog("Staked balances snapshot for block " + to_string(current_head_block_number)+" ID :" + current_head_id_block);
            userres(current_head_block_number,current_head_id_block);
        }
    }



    void u_com_impl::parse_blocks() {
        if (is_parse_blocks == false)
            return;

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();

        auto current_head_block_number = cc.fork_db_head_block_num();
        ilog("Current blocks number" + to_string(current_head_block_number));
        uint32_t end_block = current_head_block_number;
        if (end_block_parse > current_head_block_number || end_block_parse == 1)
            end_block = current_head_block_number;
        if (end_block_parse < current_head_block_number && end_block_parse != 1)
        end_block = end_block_parse;
        if(start_block > current_head_block_number)
             start_block = 1;

        ilog("start_block " + std::to_string(start_block));
        ilog("end_block " + std::to_string(end_block));
        sleep(1);

        fc::mutable_variant_object act;
        for (uint32_t i = start_block; i <= end_block; i++) {
            try {
                auto block = cc.fetch_block_by_number(i);
                ilog("Current parse block:" + to_string(i));
                act["action"] = s_DRR_Tags[DRR_TAG_ACTIONINFO];
                act["type"] = s_DRR_Tags[DRR_TAG_ALLBLOCK];
//                if(i == 1600000)
//                    break;
                parse_transactions_from_block(block, act);
            }

            catch (...) {
                elog("Error on parsing block " + std::to_string(i));
            }
        }

    }


    void u_com_impl::userres(const uint current_head_block_number, string block_id) {

        fc::mutable_variant_object wrapper;

        wrapper["action"]   = fc::variant(s_DRR_Tags[DRR_TAG_ACTIONINFO]);
        wrapper["type"]     = fc::variant(s_DRR_Tags[DRR_TAG_ACCBLOCK]);
        wrapper["command"]  = string("save_balance");
        wrapper["blocknum"] = current_head_block_number;
        wrapper["block_id"] = block_id;
        chain_apis::read_only::get_account_params params;
        auto account_list = get_all_accounts();
//        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
//
        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();
        fc::variants resources_vector;
        fc::mutable_variant_object resource_user;
        for (auto const acc_name:  account_list){
            eosio::chain::name bn = acc_name;
            params.account_name = bn;
            asset val;
            auto core_symbol = val.symbol_name();
            try {
                auto users_info = ro_api.get_account(params);
                auto cpu_weight = to_string(users_info.cpu_weight);
                auto net_weight = to_string(users_info.net_weight);
                fc::mutable_variant_object  res_user;
                res_user["cpu_weight"] = fc::variant(users_info.cpu_weight);
                res_user["net_weight"] = fc::variant(users_info.net_weight);
                resource_user[acc_name.to_string()]=fc::variant(res_user);

                auto total_resorces = users_info.total_resources;
//                elog("Account: " + acc_name.to_string() + " cpu_weight :" + cpu_weight + "Total resources : " );
//                elog("Total resources: " + fc::json::to_string(total_resorces));
            }
            catch (exception &ex)
            {
                elog(ex.what());
            }

        }

            if(th != nullptr){
                if(th->joinable()) {
                    th->join();
                    delete (th);
                }

            }

            wrapper["data"] = resource_user;
//            th = new std::thread([&](fc::mutable_variant_object data) {rb_res(data);},wrapper);
        th = new std::thread([&](fc::mutable_variant_object data) {rb_chanel(data);},wrapper);

    }

    void u_com_impl::rpc_listen() {

        try {
            string temp;
            if (q_from_rabbit->try_pop(temp)) {
                elog("RECIEVED COMMAND" + temp);
                auto block = fc::json::from_string(temp);
                if ((block.get_object().contains("parse"))) {
                    try {
                        parse_blocks();
                    }
                    catch (exception &ex) {
                        elog(ex.what());
                    }
                } else {
                    elog("Command not match");
                }
            }
        }
        catch (exception &ex) {
            elog("Error in rabbit_queue");
            elog(ex.what());

        }


    }

    void u_com_impl::rpc_server()
    {
        parse_blocks();
        runMultiThread();
    }


    void u_com_impl::parse_transactions_from_block(const eosio::chain::signed_block_ptr &block,fc::mutable_variant_object &tags)
    {

        fc::mutable_variant_object wrapper;

        auto ro_api = app().get_plugin<chain_plugin>().get_read_only_api();

        auto current_block_id = block->id();
        auto current_block_num = block->num_from_id(current_block_id);
        bool found_action = false;
        fc::mutable_variant_object var_block;
        var_block["blocknum"]=current_block_num;
        var_block["block_timestamp"]=fc::variant(block->timestamp);
        vector <fc::mutable_variant_object > action_data;
        fc::variants trx_vector;

        for (auto trs : block->transactions) {
            auto transaction = trs.trx.get<chain::packed_transaction>().get_transaction();
            auto actions = transaction.actions;
            auto transaction_id = transaction.id();
            fc::mutable_variant_object var_trx;
            var_trx["transaction_id"]=fc::variant(transaction_id);
            fc::variants actions_vector;
            for (auto action : actions) {
                if(action.account == N(eosio.token) || action.account == N(uos.activity) || action.account == N(uos.calcs)){
                    if( action.name != N(setrate)) {
                        fc::mutable_variant_object act;
//                    act["block_timestamp"]=fc::variant(timestamp);
                        act["account"] = fc::variant(action.account);
                        act["action"] = fc::variant(action.name);
                        act["data"] = ro_api.abi_bin_to_json({action.account, action.name, action.data}).args;
                        act["receiver"] = fc::variant(action.authorization);
                        found_action = true;
                        actions_vector.push_back(act);
                    }
                }

            }
            var_trx["actions"]=fc::variant(actions_vector);
            trx_vector.push_back(var_trx);
            var_trx["actions"].clear();
        }

        try{
        if(found_action) {
            if (th_data != nullptr) {
                if (th_data->joinable()) {
                    th_data->join();
                    delete (th_data);
                }
            }
            var_block["transactions"] = fc::variant(trx_vector);
            wrapper = var_block;
            wrapper["package"]= tags;

            th_data = new std::thread([&](fc::mutable_variant_object data) { rb_chanel(data);},wrapper);
//            th_data = new std::thread([&](fc::mutable_variant_object data) { rb_res(data);},wrapper);
        }
        }
        catch (exception &ex)
        {
             elog(ex.what());
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

    void u_com_impl::rb_chanel(fc::mutable_variant_object data)
    {
//            std::lock_guard<std::recursive_mutex> locker(lock_rb_chanel);

        try {
            SimplePocoHandler handler(queue_host, queue_port);
            AMQP::Connection connection(&handler, AMQP::Login(queue_login, queue_passwd), "/");
            AMQP::Channel channel(&connection);

            channel.onReady([&]() {
                if (handler.connected()) {
                    channel.publish("", "hello", fc::json::to_string(data));
//                    elog("DATA:" +fc::json::to_string(data) );
                    handler.quit();
                } else {
                    elog("Handler not connected");
                }
            });
            handler.loop();
        }
        catch (exception &ex){
            elog(ex.what());

        }
    }


    void u_com_impl::runMultiThread() {


        q_from_rabbit = make_shared<thread_safe::threadsafe_queue<string>>();

        /// 1-st thread starts
        try {
            thread t_rabbit([&](shared_ptr<thread_safe::threadsafe_queue<string>> queue_rabbit) {

                uos::rabbitmq_worker rabbitmq_input(queue_rabbit, "localhost", 5672, "guest", "guest", "/", "rpc_queue");
                elog("CREATE shared pointer");
                elog("CREATE rabbit input");
                rabbitmq_input.run();
            }, q_from_rabbit);

            if (t_rabbit.joinable())
                t_rabbit.detach();
        }
        catch (exception &ex) {
            elog(ex.what());

        }
        elog("Rabbit runs away!");

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
        cfg.add_options()
                ("queue-name", boost::program_options::value<string>()->default_value("hello"), "Name for queue" )
                ("queue-port", bpo::value<uint32_t>()->default_value(5672),"Port for queue.")
                ("queue-host", boost::program_options::value<string>()->default_value("localhost"), "Host for queue" )
                ("login", boost::program_options::value<string>()->default_value("guest"), "Login for cleints" )
                ("passwd", boost::program_options::value<string>()->default_value("guest"), "Passwd for clients" )
                ("start-block", boost::program_options::value<uint32_t >()->default_value(1), "First block for parse transaction" )
                ("end-block", boost::program_options::value<uint32_t >()->default_value(1), "Last block for parse transaction" )
                ("parse-blocks", boost::program_options::value<bool>()->default_value(false), "Parse all blocks (to current) at start plugin get transactions" )
                ;
    }

    void u_com::plugin_initialize(const variables_map& options) {
        my->_options = &options;
        my->queue_name = options.at("queue-name").as<string>();
        my->queue_port = options.at("queue-port").as<uint32_t >();
        my->queue_host = options.at("queue-host").as<string>();
        my->queue_passwd = options.at("passwd").as<string>();
        my->queue_login = options.at("login").as<string>();
        my->start_block = options.at("start-block").as<uint32_t >();
        my->end_block_parse = options.at("end-block").as<uint32_t >();
        my->is_parse_blocks = options.at("parse-blocks").as<bool>();
        my->th = nullptr;
        my->th_data = nullptr;

    }

    void u_com::plugin_startup() {

        ilog( "starting u_com" );

        my->rpc_server();

        chain::controller &cc = app().get_plugin<chain_plugin>().chain();
        cc.irreversible_block.connect([this](const auto& bsp){
            my->irreversible_block_catcher(bsp);
        });

        eosio::chain::controller &ac_block = app().get_plugin<eosio::chain_plugin>().chain();
        ac_block.accepted_block.connect([this](const auto& asp){
            my->rpc_listen();
            my->accepted_block_catcher(asp);
        });

    }

    void u_com::plugin_shutdown() {
        // OK, that's enough magic
    }

    void u_com::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}
    void u_com::rpc_server() { my->rpc_server();}
    void u_com::rpc_listen() { my->rpc_listen();}
    void u_com::accepted_block_catcher(const eosio::chain::block_state_ptr &ast) { my->accepted_block_catcher(ast);}
}
