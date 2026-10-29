#pragma once
#include <QObject>
#include <vector>
#include <utility>

namespace trainer {
// Own the composition layer's connections separately from its services. A
// receiver can outlive stack values captured by a callback; receiver lifetime
// alone therefore cannot define the end of an application session.
class SessionConnections final {
public:
    ~SessionConnections() { clear(); }
    SessionConnections() = default;
    SessionConnections(const SessionConnections&) = delete;
    SessionConnections& operator=(const SessionConnections&) = delete;
    template<class... Args> QMetaObject::Connection connect(Args&&... args) {
        auto connection=QObject::connect(std::forward<Args>(args)...);
        connections_.push_back(connection);
        return connection;
    }
    void clear() {
        for(const auto& connection:connections_)QObject::disconnect(connection);
        connections_.clear();
    }
private:
    std::vector<QMetaObject::Connection> connections_;
};
}
