#include "artworkpolicyutils.h"
ArtworkPolicyUtils::State ArtworkPolicyUtils::classify(const QHash<QString, Evidence>& evidence, bool complete)
{
    bool landscape = false, unresolved = false, square = false;
    for (const auto& entry : evidence) {
        if (!entry.hasImage) continue;
        if (!entry.resolved || !entry.size.isValid()) {
            unresolved = unresolved || entry.requested;
            continue;
        }
        if (entry.size.height() > entry.size.width()) return State::HasPortrait;
        if (entry.size.width() == entry.size.height()) square = true;
        else landscape = true;
    }
    if (square) return State::MixedOrSquare;
    if (unresolved || !landscape) return State::Unknown;
    return complete ? State::LandscapeOnly : State::LandscapeProvisional;
}
bool ArtworkPolicyUtils::isLandscape(State state)
{
    return state == State::LandscapeProvisional || state == State::LandscapeOnly;
}
