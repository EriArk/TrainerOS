#pragma once
#include "core/model/Models.h"
#include <QObject>
#include <functional>

namespace trainer {
class PreferencesRepository {
public:
    virtual ~PreferencesRepository() = default;
    virtual ShellPreferences preferences() const = 0;
    virtual void savePreferences(const ShellPreferences&, QObject*, std::function<void(QString)>) = 0;
};
}
