#include "nativedanmakulayout.h"

#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace NativeDanmakuLayout {

Schedule reflow(const Schedule &previous, QRectF surface, qreal positionMs,
                Cancellation cancelled)
{
    Schedule result = previous;
    result.items.clear();
    result.surface = surface;
    result.crowded = 0;
    result.scheduledByMotion = {};
    result.crowdedByMotion = {};
    result.maxDurationMs = 0.0;
    result.rows = 0;
    result.allowedRows = 0;
    result.validFromMs = std::max(0.0, positionMs);
    if (surface.isEmpty() || result.rowHeight <= 0.0 || result.velocity <= 0.0) {
        return result;
    }
    const qreal scale = std::max(0.4, std::min(surface.width() / 1920.0,
                                             surface.height() / 1080.0));
    const qreal top = surface.top() + 24.0 * scale;
    const qreal bottom = surface.bottom() - 48.0 * scale;
    const int rows = std::max(0, static_cast<int>((bottom - top) / result.rowHeight));
    result.rows = rows;
    if (!rows) return result;
    const int allowedRows = std::clamp(static_cast<int>(std::floor(rows *
        std::clamp(result.areaPercent, 10, 100) / 100.0 *
        std::clamp(result.density, 20, 100) / 100.0)), 1, rows);
    result.allowedRows = allowedRows;
    
    
    std::array<QVector<QVector<int>>, 3> occupied{
        QVector<QVector<int>>(rows), QVector<QVector<int>>(rows), QVector<QVector<int>>(rows)};
    const auto lanesFor = [&](Motion motion) -> QVector<QVector<int>> & {
        if (motion == Motion::Top) return occupied[1];
        if (motion == Motion::Bottom) return occupied[2];
        return occupied[0];
    };
    QSet<int> pending;
    const auto reject = [&](Motion motion) {
        ++result.crowded;
        ++result.crowdedByMotion[static_cast<int>(motion)];
    };
    const auto append = [&](Item item) {
        auto &lanes = lanesFor(item.motion);
        for (int r = item.row; r < item.row + item.span; ++r) {
            lanes[r].append(result.items.size());
        }
        result.maxDurationMs = std::max(result.maxDurationMs, item.endMs - item.startMs);
        ++result.scheduledByMotion[static_cast<int>(item.motion)];
        result.items.append(std::move(item));
    };

    if (positionMs >= 0.0) {
        for (const Item &old : previous.items) {
            if (cancelled && cancelled->load(std::memory_order_relaxed)) return {};
            if (old.startMs > positionMs) {
                if (previous.sources.at(old.source).requestedMs <= positionMs) pending.insert(old.source);
                continue;
            }
            if (old.endMs <= positionMs) continue;
            Item item = old;
            const auto &sprite = result.sprites.at(item.sprite);
            if (item.motion == Motion::Bottom) item.row += rows - previous.rows;
            const int limit = allowedRows;
            const int firstRow = item.motion == Motion::Bottom ? rows - limit : 0;
            if (item.span > limit) { reject(item.motion); continue; }
            const bool moving = item.motion == Motion::Scroll || item.motion == Motion::Reverse;
            if (moving) {
                const qreal v = (old.xTo - old.xFrom) / (old.endMs - old.startMs);
                const qreal x = old.xFrom + v * (positionMs - old.startMs) +
                                surface.left() - previous.surface.left();
                const qreal exit = item.motion == Motion::Scroll
                    ? surface.left() - sprite.size.width() - result.edgePadding
                    : surface.right() + result.edgePadding;
                const qreal remaining = (exit - x) / v;
                if (remaining <= 0.0) continue;
                
                
                item.xFrom = x - v * (positionMs - item.startMs);
                item.xTo = exit;
                item.endMs = positionMs + remaining;
            } else {
                item.xFrom = item.xTo = surface.center().x() - sprite.size.width() / 2.0;
            }
            const qreal x = item.xFrom + (item.xTo - item.xFrom) *
                (positionMs - item.startMs) / (item.endMs - item.startMs);
            const int preferred = std::clamp(item.row, firstRow, firstRow + limit - item.span);
            int chosen = -1;
            
            
            const auto &lanes = lanesFor(item.motion);
            for (int n = 0; n <= limit - item.span; ++n) {
                const int row = firstRow + (preferred - firstRow + n) % (limit - item.span + 1);
                bool fits = true;
                for (int r = row; r < row + item.span && fits; ++r) {
                    for (int index : lanes.at(r)) {
                        const Item &other = result.items.at(index);
                        const qreal otherX = other.xFrom + (other.xTo - other.xFrom) *
                            (positionMs - other.startMs) / (other.endMs - other.startMs);
                        
                        
                        
                        const qreal velocity = (item.xTo - item.xFrom) / (item.endMs - item.startMs);
                        const qreal otherVelocity = (other.xTo - other.xFrom) /
                                                    (other.endMs - other.startMs);
                        const qreal remaining = std::min(item.endMs, other.endMs) - positionMs;
                        const qreal relativeNow = x - otherX;
                        const qreal relativeEnd = relativeNow + (velocity - otherVelocity) * remaining;
                        if (!(std::max(relativeNow, relativeEnd) + sprite.size.width() + result.gap <= 0.0 ||
                              std::min(relativeNow, relativeEnd) >=
                                  result.sprites.at(other.sprite).size.width() + result.gap)) {
                            fits = false;
                            break;
                        }
                    }
                }
                if (fits) { chosen = row; break; }
            }
            if (chosen < 0) { reject(item.motion); continue; }
            item.row = chosen;
            item.y = top + chosen * result.rowHeight;
            append(item);
        }
    }

    for (int sourceId = 0; sourceId < result.sources.size(); ++sourceId) {
        if (cancelled && cancelled->load(std::memory_order_relaxed)) return {};
        const Source &source = result.sources.at(sourceId);
        const bool moving = source.motion == Motion::Scroll || source.motion == Motion::Reverse;
        
        
        if (positionMs >= 0.0 && source.requestedMs <= positionMs && !pending.contains(sourceId)) continue;
        const qreal requested = positionMs >= 0.0
            ? std::max(source.requestedMs, positionMs) : source.requestedMs;
        const Sprite &sprite = result.sprites.at(source.sprite);
        const int span = std::max(1, static_cast<int>(std::ceil(sprite.size.height() / result.rowHeight)));
        const int limit = allowedRows;
        if (span > limit) { reject(source.motion); continue; }
        const qreal duration = moving
            ? (surface.width() + 2.0 * result.edgePadding + sprite.size.width()) / result.velocity
            : 4200.0;
        const qreal queueLimit = moving ? std::min(5000.0, duration * 0.42) : 1800.0;
        const qreal entry = source.motion == Motion::Reverse
            ? surface.left() - sprite.size.width() - result.edgePadding
            : surface.right() + result.edgePadding;
        auto &lanes = lanesFor(source.motion);
        for (auto &lane : lanes) {
            lane.erase(std::remove_if(lane.begin(), lane.end(), [&](int index) {
                return result.items.at(index).endMs <= requested;
            }), lane.end());
        }
        qreal earliest = std::numeric_limits<qreal>::max();
        int selectedRow = -1;
        for (int candidate = 0; candidate <= limit - span; ++candidate) {
            const int row = source.motion == Motion::Bottom ? rows - span - candidate : candidate;
            qreal available = requested;
            for (int r = row; r < row + span; ++r) {
                for (int index : std::as_const(lanes[r])) {
                    const Item &prior = result.items.at(index);
                    qreal release = prior.endMs;
                    if (moving && prior.motion == source.motion) {
                        const qreal distance = source.motion == Motion::Scroll
                            ? prior.xFrom + result.sprites.at(prior.sprite).size.width() + result.gap - entry
                            : entry + sprite.size.width() + result.gap - prior.xFrom;
                        release = prior.startMs + distance / result.velocity;
                    }
                    available = std::max(available, release);
                }
            }
            if (available < earliest) { earliest = available; selectedRow = row; }
            if (available <= requested) break;
        }
        if (selectedRow < 0 || earliest - requested > queueLimit) { reject(source.motion); continue; }
        Item item;
        item.motion = source.motion;
        item.source = sourceId;
        item.sprite = source.sprite;
        item.row = selectedRow;
        item.span = span;
        item.startMs = earliest;
        item.endMs = earliest + duration;
        item.y = top + selectedRow * result.rowHeight;
        item.xFrom = entry;
        item.xTo = surface.left() - sprite.size.width() - result.edgePadding;
        if (source.motion == Motion::Reverse) item.xTo = surface.right() + result.edgePadding;
        else if (!moving) item.xFrom = item.xTo = surface.center().x() - sprite.size.width() / 2.0;
        append(item);
    }
    
    
    std::stable_sort(result.items.begin(), result.items.end(), [](const Item &a, const Item &b) {
        return a.startMs != b.startMs ? a.startMs < b.startMs : a.source < b.source;
    });
    return result;
}

} 
