#pragma once
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>

namespace eosio {

    using namespace appbase;

    class u_com : public appbase::plugin<u_com> {
    public:
        u_com();
        virtual ~u_com();

        APPBASE_PLUGIN_REQUIRES()
        virtual void set_program_options(options_description&, options_description& cfg) override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        void irreversible_block_catcher(const chain::block_state_ptr& bst);
        void parse_blocks();

    private:
        std::unique_ptr<class u_com_impl> my;
    };

}