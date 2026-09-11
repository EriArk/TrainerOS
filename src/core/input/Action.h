#pragma once
#include <QMetaType>

namespace trainer {
enum class Action {
    Up, Down, Left, Right, Confirm, Back, SystemMenu,
    PreviousPage, NextPage, ToggleContinue, Secondary
};
}
Q_DECLARE_METATYPE(trainer::Action)
