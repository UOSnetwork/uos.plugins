//
// Created by anton on 26.11.18.
//

#pragma once

#include <memory>
#include "SimplePocoHandler.h"
#include "thread_safe.hpp"

namespace uos{

    using std::string;

    struct rabbit_params{
        string   rabbit_host;
        uint16_t rabbit_port;
        string   rabbit_login;
        string   rabbit_password;
        string   rabbit_path;
        string   rabbit_input_queue;
        string   rabbit_output_queue;
    };

    class rabbitmq_worker{

        std::shared_ptr<thread_safe::threadsafe_queue<string>> rabbit_queue;

        SimplePocoHandler poco_handler;
        AMQP::Connection rb_connection;
        AMQP::Channel rb_channel;
        string rb_queue_name;


    public:
        rabbitmq_worker(std::shared_ptr<thread_safe::threadsafe_queue<string>> &rb_out_queue,
                        const string &rb_address,
                        const uint16_t    &rb_port,
                        const string &rb_login,
                        const string &rb_password,
                        const string &rb_vhost,
                        const string &rb_queue):
                poco_handler(rb_address,rb_port),
                rb_connection(&poco_handler, AMQP::Login(rb_login, rb_password), rb_vhost),
                rb_channel(&rb_connection)
                            {
                                rb_queue_name = rb_queue;
                                rb_channel.declareQueue(rb_queue);
                                rabbit_queue = rb_out_queue;
                            };

        rabbitmq_worker(std::shared_ptr<thread_safe::threadsafe_queue<string>> &rb_out_queue, rabbit_params params):
                poco_handler(params.rabbit_host,params.rabbit_port),
                rb_connection(&poco_handler, AMQP::Login(params.rabbit_login, params.rabbit_password), params.rabbit_path),
                rb_channel(&rb_connection)
                            {
                                rb_queue_name = params.rabbit_input_queue;
                                rb_channel.declareQueue(params.rabbit_input_queue);
                                rabbit_queue = rb_out_queue;

                            };
        rabbitmq_worker(const rabbitmq_worker&) = delete;
        rabbitmq_worker& operator=(const rabbitmq_worker&) = delete;
        ~rabbitmq_worker(){poco_handler.quit();}

        void run();
        void stop();

    };

}
