
#include <eosio/uos_blocks_exporter/uos_blocks_exporter.hpp>
#include <eosio/uos_blocks_exporter/mongo_worker.hpp>
#include <eosio/uos_blocks_exporter/thread_safe.hpp>
#include <fc/io/json.hpp>

namespace uos_plugins{

    static appbase::abstract_plugin& _uos_BE = app().register_plugin<uos_BE>();

    class uos_BE_impl {
    public:
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

    void uos_BE_impl::irreversible_block_catcher(const eosio::chain::block_state_ptr &bsp) {

    }

    void uos_BE_impl::accepted_block_catcher(const eosio::chain::block_state_ptr &asp) {
//        fc::mutable_variant_object block;
//        fc::mutable_variant_object actions;
//        for(auto item_trx : asp->trxs){
//            for( auto  act : item_trx->packed_trx->get_transaction().actions){
//                if(allowed_actions[act.account.to_string()].count(act.name.to_string())){
//
//                }
//            }
//        }
    }

    void uos_BE_impl::accepted_transaction_catcher(const eosio::chain::transaction_metadata_ptr &atm) {

    }

    bool fill_inline_traces_console(eosio::chain::action_trace& action_trace){
        bool ret = false;
        if(action_trace.console.length()==0){
            action_trace.console = fc::json::to_string(app().get_plugin<eosio::chain_plugin>().get_read_only_api().abi_bin_to_json({action_trace.act.account,action_trace.act.name,action_trace.act.data}).args);
            wlog(action_trace.console);

            for(auto &item : action_trace.inline_traces){
                fill_inline_traces_console(item);
                ret = true;
            }
        }
        return ret;
    }

    void console_to_variant(fc::mutable_variant_object &val){

        auto itr = val.find("console");
        if(itr!=val.end()){
            if(itr->value().as_string().length()>0){
                val["data"]=fc::json::from_string(itr->value().as_string());
            }
        }

        itr = val.find("inline_traces");
        if(itr!=val.end()){
            ///todo
        }
    }

    void uos_BE_impl::applied_transaction_catcher(const eosio::chain::transaction_trace_ptr &att) {
        fc::variants actions;
//        fc::variant act;
//        act = *att;
//        std::cout<<fc::json::to_string(act)<<std::endl<<std::endl;
        bool out = false;
        for(auto item : att->action_traces){
            if(item.act.account==N(eosio) && item.act.name==N(onblock)){
                continue;
            }
            out  |= fill_inline_traces_console(item);
            auto action = fc::mutable_variant_object(item);
            console_to_variant(action);
            actions.emplace_back(action);
        }
        if(actions.size()>0){
            std::cout<<fc::json::to_string(actions)<<std::endl<<std::endl;
            if(out)
                std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    uos_BE::uos_BE():my(new uos_BE_impl()) {}
    uos_BE::~uos_BE(){}

    void uos_BE::irreversible_block_catcher(const eosio::chain::block_state_ptr &bst) { my->irreversible_block_catcher(bst);}
    void uos_BE::accepted_block_catcher(const eosio::chain::block_state_ptr &ast) { my->accepted_block_catcher(ast);}
    void uos_BE::set_program_options(boost::program_options::options_description &,
                                     boost::program_options::options_description &cfg) {}



    void uos_BE::plugin_shutdown() {

    }
    void uos_BE::plugin_startup() {

        eosio::chain::controller &cc = app().get_plugin<eosio::chain_plugin>().chain();

        cc.irreversible_block.connect([this](const auto& bsp){
            my->irreversible_block_catcher(bsp);
        });
        cc.accepted_block.connect([this](const auto& asp){
            my->accepted_block_catcher(asp);
        });
        cc.accepted_transaction.connect([this](const auto& atm){
            my->accepted_transaction_catcher(atm);
        });
        cc.applied_transaction.connect([this](const auto& att){
            my->applied_transaction_catcher(att);
        });
    }
    void uos_BE::plugin_initialize(const boost::program_options::variables_map &options) {

    }
}