//
// Created by coodi on 06.11.18.
//

#pragma once

#include <fc/variant_object.hpp>
#include <queue>

namespace eosio {
    using namespace std;

    struct trx_to_run{
        string account;
        string action;
        fc::mutable_variant_object data;
        string pub_key;
        string priv_key;
        string acc_from;
        trx_to_run();
        trx_to_run(string acc, string act, fc::mutable_variant_object d, string pubk, string privk, string accfrom){
            account = acc;
            action = act;
            data = d;
            pub_key = pubk;
            priv_key = privk;
            acc_from = accfrom;
        }
    };

    typedef queue<trx_to_run> transaction_queue;
}
