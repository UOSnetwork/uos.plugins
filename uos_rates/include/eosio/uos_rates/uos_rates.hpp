#pragma once
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>

namespace eosio {

    using namespace appbase;

    class uos_rates : public appbase::plugin<uos_rates> {
    public:
        uos_rates();
        virtual ~uos_rates();

        APPBASE_PLUGIN_REQUIRES()
        virtual void set_program_options(options_description&, options_description& cfg) override;

        void plugin_initialize(const variables_map& options);
        void plugin_startup();
        void plugin_shutdown();

        void irreversible_block_catcher(const chain::block_state_ptr& bst);

    private:
        std::unique_ptr<class uos_rates_impl> my;
    };

}