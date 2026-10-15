#pragma once
#include "features/social/SocialController.h"

namespace trainer {
class HomeSocialFixture {
public:
    static void publish(SocialController& social, QVariantMap snapshot) {
        social.receive(0, std::move(snapshot));
    }
};
}
