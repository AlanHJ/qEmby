#ifndef PLAYERLAUNCHCONTEXT_H
#define PLAYERLAUNCHCONTEXT_H

#include "mediaitem.h"
#include "playbackinfo.h"

struct PlayerLaunchContext {
    MediaItem mediaItem;
    MediaSourceInfo selectedSource;
};
Q_DECLARE_METATYPE(PlayerLaunchContext)

#endif 
