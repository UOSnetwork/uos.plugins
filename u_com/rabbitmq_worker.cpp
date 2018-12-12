//
// Created by anton on 26.11.18.
//
#include <thread>
#include <fc/exception/exception.hpp>
#include "rabbitmq_worker.hpp"

namespace uos {


    void rabbitmq_worker::run() {
        if (rabbit_queue == nullptr) {
            std::cout << "Rabbit queue not initialized" << std::endl;
            return;
        }

        rb_channel.consume(rb_queue_name, AMQP::noack).onReceived(
                [&](const AMQP::Message &message,
                    uint64_t deliveryTag,
                    bool redelivered) {

                    try {
                        auto received_json = std::string(message.body(), message.bodySize());
//                        auto received_data = message.message();
//                        std::cout << " [x] Received _data"
//                                  << received_data
//                                  << std::endl;
                        std::cout << " [x] Received "
                                  << received_json
                                  << std::endl;
                        if (received_json.size() > 0) {
                            rabbit_queue->push(std::string(message.body(), message.bodySize()));
                            std::cout << rabbit_queue->size() << " - " << std::endl;
//                        if(rabbit_queue->size()>100000)
//                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    }
                    catch (std::exception &ex)
                    {
                     std::cout<<"\n Error handler queue \n";
                     std::cout<<ex.what();
                    }
                });
        std::cout << " [*] Waiting for messages. To exit press CTRL-C" << std::endl;
        poco_handler.loop();

    }

    void rabbitmq_worker::stop() {
        std::cout << "Rabbit quit" << std::endl;
        poco_handler.quit();
    }

}
