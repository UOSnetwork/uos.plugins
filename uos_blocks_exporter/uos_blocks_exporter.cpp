
#include <eosio/uos_blocks_exporter/uos_blocks_exporter.hpp>
#include <eosio/uos_blocks_exporter/mongo_worker.hpp>
#include <eosio/uos_blocks_exporter/thread_safe.hpp>
#include <fc/io/json.hpp>




namespace uos_plugins{


#define  UOS_DEFAULT_WHITELIST "{\"whitelist\":[]}"
#define  UOS_DEFAULT_BLACKLIST "{\"blacklist\":[{\"contract\":\"eosio\",\"action\":\"onblock\"},{\"contract\":\"uos.calcs\",\"action\":\"setrate\"},{\"contract\":\"uos.calcs\",\"action\":\"addsum\"}]}"

    static appbase::abstract_plugin& _uos_BE = app().register_plugin<uos_BE>();

    class uos_BE_impl {
    public:
        uos_BE_impl();
        ~uos_BE_impl(){};
        void irreversible_block_catcher(const eosio::chain::block_state_ptr& bsp);
        void accepted_block_catcher(const eosio::chain::block_state_ptr& asp);
        void accepted_transaction_catcher(const eosio::chain::transaction_metadata_ptr& atm);
        void applied_transaction_catcher(const eosio::chain::transaction_trace_ptr & att);
        void mongo_init();
        void queue_processor();

        boost::program_options::variables_map _options;
        friend class uos_BE;
        std::map<std::string,std::set<std::string>> allowed_actions;
    private:
        std::atomic_bool stop;
        std::shared_ptr<uos::mongo_worker> mongo;
        std::shared_ptr<std::thread> mongo_thread;
        uos::mongo_last_state last_state;
//        std::shared_ptr<thread_safe::threadsafe_queue<std::string>> irreversible_blocks_queue;
        std::shared_ptr<thread_safe::threadsafe_queue<std::string>> accepted_blocks_queue;
        std::shared_ptr<std::set<std::string>> white_list;
        std::shared_ptr<std::set<std::string>> black_list;
        uos::mongo_params MongoConnectionParams;
    };

    uos_BE_impl::uos_BE_impl(){

//        accepted_blocks_queue = std::make_shared<thread_safe::threadsafe_queue<std::string>>();
        white_list = std::make_shared<std::set<std::string>>();
        black_list = std::make_shared<std::set<std::string>>();


    };

    void uos_BE_impl::mongo_init() {
        mongo = std::make_shared<uos::mongo_worker>(MongoConnectionParams);
    }

    void uos_BE_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        mongo->set_irreversible_block(bsp->block_num,bsp->block->id());
        last_state.mongo_irrblockid = bsp->block->id();
        last_state.mongo_irrblocknum = bsp->block_num;
        mongo->set_last_state(last_state);
//        elog(std::string("-"+std::string(bsp->block->id())+" "+std::to_string(bsp->block_num)));
    }

    void uos_BE_impl::accepted_block_catcher(const eosio::chain::block_state_ptr &asp) {

    }

    void uos_BE_impl::accepted_transaction_catcher(const eosio::chain::transaction_metadata_ptr &atm) {

    }

    fc::variant fill_inline_traces(
            eosio::chain::action_trace& action_trace,
            std::shared_ptr<std::set<eosio::chain::name>> contracts= nullptr,
            std::shared_ptr<std::set<eosio::chain::name>> actors = nullptr){
        if(contracts){
            contracts->insert(action_trace.receipt.receiver);
        }

        fc::mutable_variant_object action_mvariant(fc::variant(static_cast<eosio::chain::base_action_trace>(action_trace)));
        action_mvariant["act_data"] = app().get_plugin<eosio::chain_plugin>().get_read_only_api().abi_bin_to_json({action_trace.act.account,action_trace.act.name,action_trace.act.data}).args;
//        if(actors){
//            action_mvariant["act_data"].
//        }
        fc::variants inline_traces;
        for(auto item: action_trace.inline_traces){
            inline_traces.emplace_back(fill_inline_traces(item,contracts));
        }
        action_mvariant["inline_traces"]=inline_traces;
        return action_mvariant;

    }

    void uos_BE_impl::applied_transaction_catcher(const eosio::chain::transaction_trace_ptr &att) {
        fc::variants actions;
        std::shared_ptr<std::set<eosio::chain::name>> constracts = std::make_shared<std::set<eosio::chain::name>>();
        try{
            if(att->producer_block_id) {
                last_state.mongo_blockid = fc::variant(att->producer_block_id).as_string();
                last_state.mongo_blocknum = att->block_num;
                mongo->set_last_state(last_state);
            }
        }
        catch (mongocxx::exception &ex){
            elog(ex.what());
        }
        catch(...){

        }
        for(auto item : att->action_traces){
            if( black_list->size() > 0 ){
                if ( black_list->find(item.act.account.to_string()+"."+item.act.name.to_string()) !=black_list->end()){
                    continue;
                }
            }
            if( white_list->size() > 0 ){
                if ( white_list->find(item.act.account.to_string()+"."+item.act.name.to_string()) != white_list->end()){
                    wlog("whitelist");
                }
            }
            else{
                wlog("whitelist is empty");
            }
            actions.emplace_back(fill_inline_traces(item,constracts));
        }
        if(actions.size()>0){
//            wlog(fc::json::to_string(actions));
            bool consists_inlines = false;
            for(auto item: actions){
                if(item.get_object().contains("inline_traces") &&
                        item["inline_traces"].get_array().size()>0){
                    consists_inlines = true;
                    break;
                }
            }
            fc::mutable_variant_object mblock;
            mblock["blocknum"]      = att->block_num;
            mblock["blockid"]       = att->producer_block_id;
            mblock["trxid"]         = att->id;
            mblock["account"]       = actions[0]["act"]["authorization"].get_array()[0]["actor"].as_string();
            mblock["irreversible"]  = false;
            mblock["actions"]       = actions;
            mblock["blocktime"]     = att->block_time;

            try {
                mongo->put_action_traces(fc::json::to_string(mblock));
                if(!constracts->empty()){
                    fc::mutable_variant_object mcontr;
                    mcontr["blocknum"] = att->block_num;
                    mcontr["blockid"] = att->producer_block_id;
                    mcontr["trxid"] = att->id;
                    for(auto item : *constracts){
                        mcontr["account"] = item.to_string();
                        mongo->put_trx_contracts(fc::json::to_string(mcontr));
                    }
                }
                accepted_blocks_queue->push(fc::json::to_string(mblock));
            }
            catch (mongocxx::exception &ex){
                elog(ex.what());
            }
        }
    }

    void uos_BE_impl::queue_processor() {
        while(!stop) {
            if(accepted_blocks_queue!= nullptr){
                break;
            }
            elog("queue not initialized");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::string item;
        while((!stop)||(!accepted_blocks_queue->empty())) {
            if(stop){
                std::cout<<accepted_blocks_queue->size()<<std::endl; //todo: remove this
            }
            if(!accepted_blocks_queue->try_pop(item)){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else {
                //elog(item); //todo: remove this
            }

        }
    }

///
    uos_BE::uos_BE() {    }
    uos_BE::~uos_BE(){}

    void uos_BE::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}
    void uos_BE::accepted_block_catcher(const eosio::chain::block_state_ptr &ast) { my->accepted_block_catcher(ast);}

    void uos_BE::set_program_options(boost::program_options::options_description &,
                                     boost::program_options::options_description &cfg) {
        cfg.add_options()
                ("uos-mongo-uri",      boost::program_options::value<std::string>()->default_value("mongodb://localhost"), "MongoDB URI/ Exmaple: mongodb://user:password@address:port")
                ("uos-mongo-database", boost::program_options::value<std::string>()->default_value("uos-database"), "Database for collections")

                ("uos-mongo-whitelist-contracts", boost::program_options::value<std::string>()->default_value(UOS_DEFAULT_WHITELIST),"what accounts and actions should be saved")
                ("uos-mongo-blacklist-contracts", boost::program_options::value<std::string>()->default_value(UOS_DEFAULT_BLACKLIST),"what accounts and actions should NOT(!) be saved")
                ;
    }



    void uos_BE::plugin_shutdown() {
        my->stop = true;
        if(my->mongo_thread != nullptr){
            if(my->mongo_thread->joinable()){
                elog("join");
                my->mongo_thread->join();
            }
        }

    }


    void uos_BE::plugin_startup() {
        if(startup) {
            try {
                my->last_state = my->mongo->get_last_state();
            }
            catch (mongocxx::exception &ex){
                elog(ex.what());
            }
            catch (...){
                elog("error");
            }

            eosio::chain::controller &cc = app().get_plugin<eosio::chain_plugin>().chain();

            cc.irreversible_block.connect([this](const auto &bsp) {
                my->irreversible_block_catcher(bsp);
            });
            cc.accepted_block.connect([this](const auto &asp) {
                my->accepted_block_catcher(asp);
            });
            cc.accepted_transaction.connect([this](const auto &atm) {
                my->accepted_transaction_catcher(atm);
            });
            cc.applied_transaction.connect([this](const auto &att) {
                my->applied_transaction_catcher(att);
            });
            /// prepare second thread
            my->stop = false;
            my->accepted_blocks_queue = std::make_shared<thread_safe::threadsafe_queue<std::string>>();
            my->mongo_thread = std::make_shared<std::thread>([this]{my->queue_processor();});
        }
        else{
            elog("UOS blocks exporter disabled. Error in connection to mongo DB");
        }
    }
    void uos_BE::plugin_initialize(const boost::program_options::variables_map &options) {

        try {

            my = std::make_unique<uos_BE_impl>();

            my->_options = &options;

            my->MongoConnectionParams.mongo_uri =               options.at("uos-mongo-uri").as<std::string>();
            my->MongoConnectionParams.mongo_connection_name =   options.at("uos-mongo-database").as<std::string>();
            my->MongoConnectionParams.mongo_db_action_traces =  "action_traces";
            my->MongoConnectionParams.mongo_db_contracts     =  "contracts";

            my->mongo_init();

            startup = true;
        }
        catch(...){
            startup = false;
        }
        if(startup){
            try{
                auto whitelist = fc::json::from_string(options.at("uos-mongo-whitelist-contracts").as<std::string>());
                auto blacklist = fc::json::from_string(options.at("uos-mongo-blacklist-contracts").as<std::string>());
                wlog(fc::json::to_string(whitelist));
                wlog(fc::json::to_string(blacklist));
                if( whitelist.get_object()["whitelist"].get_type() != fc::variant::type_id::array_type)
                    throw std::runtime_error("Whitelist not contains 'whitelist' or 'whitelist' is not array");
                if( blacklist.get_object()["blacklist"].get_type() != fc::variant::type_id::array_type)
                    throw std::runtime_error("Blacklist not contains 'blacklist' or 'blacklist' is not array");
                for(auto item : whitelist.get_object()["whitelist"].get_array()){
                    my->white_list->emplace(item.get_object()["contract"].as<std::string>() +"." + item.get_object()["action"].as<std::string>());
                }
                for(auto item : blacklist.get_object()["blacklist"].get_array()){
                    my->black_list->emplace(item.get_object()["contract"].as<std::string>() +"." + item.get_object()["action"].as<std::string>());
                }
            }
            catch(std::exception &e){
                elog(e.what());
                elog("Error in parsing white/black lists, using default lists");
                auto whitelist = fc::json::from_string(UOS_DEFAULT_WHITELIST);
                auto blacklist = fc::json::from_string(UOS_DEFAULT_BLACKLIST);
                for(auto item : whitelist.get_object()["whitelist"].get_array()){
                    my->white_list->emplace(item.get_object()["contract"].as<std::string>() +"." + item.get_object()["action"].as<std::string>());
                }
                for(auto item : blacklist.get_object()["blacklist"].get_array()){
                    my->black_list->emplace(item.get_object()["contract"].as<std::string>() +"." + item.get_object()["action"].as<std::string>());
                }
            }

            for(auto item : *my->black_list){
                wlog(item);
            }

        }
        else{
            elog("UOS blocks exporter disabled. Error in connection to mongo DB");
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
}