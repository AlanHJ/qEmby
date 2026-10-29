#pragma once
#include <QHash>
#include <QSize>
#include <QString>
namespace ArtworkPolicyUtils {
enum class State { Unknown, LandscapeProvisional, LandscapeOnly, HasPortrait, MixedOrSquare };
enum class Layout { Default, Landscape, Square, Portrait };
struct Evidence { QString identity; QSize size; bool hasImage = true; bool resolved = false; bool requested = false; };
State classify(const QHash<QString, Evidence>& evidence, bool complete);
bool isLandscape(State state);
}
