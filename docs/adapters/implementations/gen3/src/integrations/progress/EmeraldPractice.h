#pragma once
#include "core/model/GameProgress.h"
#include <QJsonObject>

namespace trainer {
QJsonObject emeraldPracticeMember(const GameProgress&,int index);
struct PracticePair { QJsonObject input; QString error; };
// A frozen semantic copy. No file paths, write permissions or raw save bytes.
PracticePair emeraldPracticePair(const GameProgress&, int first, int second);
}
