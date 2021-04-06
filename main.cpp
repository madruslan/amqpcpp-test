#include <chrono>
#include <string>
#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <amqpcpp/libev.h>

class Connection {
    struct ev_loop* _loop = EV_DEFAULT;
    long _corrId = 1;

    std::mutex _channelMutex;
    std::unique_ptr<AMQP::TcpChannel> _channel;

    std::mutex _answerMutex;
    std::condition_variable _answerLock;

    std::mutex _readyMutex;
    std::condition_variable _readyLock;

    ev_async _asyncWatcher;
    std::shared_ptr<std::thread> _mainThread;

public:
    Connection() = default;

    void start() {
        ev_async_init(&_asyncWatcher, [](EV_P_ ev_async*, int) {ev_break(loop, EVBREAK_ONE);});
        ev_async_start(_loop, &_asyncWatcher);

        _mainThread = std::make_shared<std::thread>([this]() -> void {
            AMQP::LibEvHandler handler(_loop);
            AMQP::TcpConnection connection(&handler, {"amqp://127.0.0.1"});
            if (!connection.usable()) abort();
            {
                std::unique_lock lock(_channelMutex);
                _channel = std::make_unique<AMQP::TcpChannel>(&connection);
                _channel->onReady([this]() {_readyLock.notify_one();});
            }
            ev_run(_loop);
        });
    }

    void stop() {
        std::cout << "stop connection" << std::endl;
        ev_async_send(_loop, &_asyncWatcher);
        _mainThread->join();
    }

    bool isReady() {
        std::unique_lock<std::mutex> lock(_readyMutex);
        return _readyLock.wait_for(lock, std::chrono::seconds(3)) != std::cv_status::timeout;
    }

    void send(const std::string& requestKey, const std::string& message) {
        {
            std::lock_guard lock(_channelMutex);
            _channel->declareQueue("", AMQP::exclusive)
                    .onSuccess([this, requestKey, message](const std::string &replyQueue, uint32_t, uint32_t) {
                                std::cout << replyQueue << std::endl;

                                AMQP::Envelope env(message.c_str(), message.size());
                                env.setCorrelationID(std::to_string(_corrId++));
                                env.setReplyTo(replyQueue);

                                std::lock_guard lock(_channelMutex);
                                _channel->consume(replyQueue)
                                        .onReceived([this, replyQueue](const AMQP::Message& message, uint64_t deliveryTag, bool) {
                                            std::cout << "answer: " << std::string(message.body(), message.bodySize()) << std::endl;
                                            {
                                                std::lock_guard lock(_channelMutex);
                                                _channel->ack(deliveryTag);
                                                _channel->removeQueue(replyQueue);
                                            }
                                            _answerLock.notify_one();
                                        });
                                _channel->publish("test_exchange", requestKey, env);
                            });
        }
        std::unique_lock<std::mutex> lock(_answerMutex);
        _answerLock.wait(lock);
    }
};

int main() {
    Connection connection;
    connection.start();
    if (!connection.isReady()) abort();
    for (int i = 0; i < 10000; i++) {
        connection.send("test_key", "test message");
    }
    connection.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50000));
    return 0;
}
