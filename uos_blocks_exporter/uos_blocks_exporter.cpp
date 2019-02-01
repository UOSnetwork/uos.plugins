
#include <eosio/uos_blocks_exporter/uos_blocks_exporter.hpp>
#include <eosio/uos_blocks_exporter/mongo_worker.hpp>
#include <eosio/uos_blocks_exporter/thread_safe.hpp>
#include <fc/io/json.hpp>

namespace uos_plugins{

    static appbase::abstract_plugin& _uos_BE = app().register_plugin<uos_BE>();

    class uos_BE_impl {
    public:
        uos_BE_impl();
        ~uos_BE_impl(){};
        void irreversible_block_catcher(const eosio::chain::block_state_ptr& bsp);
        void accepted_block_catcher(const eosio::chain::block_state_ptr& asp);
        void accepted_transaction_catcher(const eosio::chain::transaction_metadata_ptr& atm);
        void applied_transaction_catcher(const eosio::chain::transaction_trace_ptr & att);

        boost::program_options::variables_map _options;
        friend class uos_BE;
        std::map<std::string,std::set<std::string>> allowed_actions;
    private:
        std::shared_ptr<uos::mongo_worker> mongo;
        std::shared_ptr<thread_safe::threadsafe_queue<std::string>> irreversible_blocks_queue;
        std::shared_ptr<thread_safe::threadsafe_queue<std::string>> accepted_blocks_queue;
        uos::mongo_params MongoConnectionParams;
    };

    uos_BE_impl::uos_BE_impl(){
        uos::mongo_params params;
        params.mongo_uri = "mongodb://localhost";
        params.mongo_user = "";
        params.mongo_password = "";
        params.mongo_connection_name = "testbase";
        params.mongo_db_action_traces = "action_traces";

        accepted_blocks_queue = std::make_shared<thread_safe::threadsafe_queue<std::string>>();
        mongo = std::make_shared<uos::mongo_worker>(params);

    };

    void uos_BE_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {
        auto ret = mongo->set_irreversible_block(bsp->block_num,bsp->block->id());
//        elog(std::string("-"+std::string(bsp->block->id())+" "+std::to_string(bsp->block_num)));
    }

    void uos_BE_impl::accepted_block_catcher(const eosio::chain::block_state_ptr &asp) {

    }

    void uos_BE_impl::accepted_transaction_catcher(const eosio::chain::transaction_metadata_ptr &atm) {

    }

    fc::variant fill_inline_traces(eosio::chain::action_trace& action_trace){
        fc::mutable_variant_object action_mvariant(fc::variant(static_cast<eosio::chain::base_action_trace>(action_trace)));
        action_mvariant["act_data"] = app().get_plugin<eosio::chain_plugin>().get_read_only_api().abi_bin_to_json({action_trace.act.account,action_trace.act.name,action_trace.act.data}).args;
        fc::variants inline_traces;
        for(auto item: action_trace.inline_traces){
            inline_traces.emplace_back(fill_inline_traces(item));
        }
        action_mvariant["inline_traces"]=inline_traces;
        return action_mvariant;

    }

    void uos_BE_impl::applied_transaction_catcher(const eosio::chain::transaction_trace_ptr &att) {
        fc::variants actions;
        for(auto item : att->action_traces){
            if(item.act.account==N(eosio) && item.act.name==N(onblock)){
                continue;
            }
            actions.emplace_back(fill_inline_traces(item));
        }
        if(actions.size()>0){
            wlog(fc::json::to_string(actions));
            bool consists_inlines = false;
            for(auto item: actions){
                if(item.get_object().contains("inline_traces") &&
                        item["inline_traces"].get_array().size()>0){
                    consists_inlines = true;
                    break;
                }
            }
//            if(consists_inlines)
//                std::this_thread::sleep_for(std::chrono::seconds(10));
            fc::mutable_variant_object mblock;
            mblock["blocknum"] = att->block_num;
            mblock["blockid"] = att->producer_block_id;
            mblock["irreversible"] = false;
            mblock["actions"] = actions;
            mblock["blocktime"] = att->block_time;

            try {
                mongo->put_action_traces(fc::json::to_string(mblock));
            }
            catch (mongocxx::exception &ex){
                elog(ex.what());
            }
        }
    }

    uos_BE::uos_BE() {    }
    uos_BE::~uos_BE(){}

    void uos_BE::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}
    void uos_BE::accepted_block_catcher(const eosio::chain::block_state_ptr &ast) { my->accepted_block_catcher(ast);}
    void uos_BE::set_program_options(boost::program_options::options_description &,
                                     boost::program_options::options_description &cfg) {}



    void uos_BE::plugin_shutdown() {

    }
    void uos_BE::plugin_startup() {
        if(startup) {

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
        }
        else{
            wlog("UOS blocks exporter disabled. Error in connection to mongo DB");
        }
    }
    void uos_BE::plugin_initialize(const boost::program_options::variables_map &options) {
        try {
            my = std::make_unique<uos_BE_impl>();
            startup = true;
        }
        catch(...){
            startup = false;
        }
        if(startup){

        }
        else{
            wlog("UOS blocks exporter disabled. Error in connection to mongo DB");
        }
    }
}