#include "deathtiming.h"

namespace pacman::core {

namespace {
// Every number below was measured on the oracle (tools/death_model_analyse.py, docs/M8_PLAN.md "033-3b"); the shifted frame routine costs more the more it has to shift:
// the extra cost depends on s = x mod 7 (0..6).
constexpr std::int64_t kFirstClear = 761'750;                                   // stillness after the catching tick
constexpr std::int64_t kClearSteps[4] = {1'880, 1'880, 1'880, 1'539};          // between the five erases
constexpr std::int64_t kClearToImage = 1'510;
constexpr std::int64_t kImageToImageClear = 777'378;                            // the second stillness
constexpr std::int64_t kImageClearToFrame[7] = {3'482, 4'163, 4'845, 5'526, 6'207, 2'119, 2'801};
constexpr std::int64_t kFrameToClear[6] = {148'763, 149'453, 152'837, 159'029, 162'197, 136'416};   // draw + sound + wait (s = 0, 1, 5, 6)
constexpr std::int64_t kFrameToClearForShift234 = 30;                           // s = 2, 3, 4 take this much longer
constexpr std::int64_t kPageCrossingMean = 28;                                  // 0 or 56 more depending on the screen contents/place; the middle halves the largest error
constexpr std::int64_t kClearToNextFrame[7] = {3'504, 4'185, 4'867, 5'548, 6'229, 2'141, 2'823};
constexpr std::int64_t kFrameToSound = 1'522;
constexpr std::int64_t kLastClearToRebuild = 316'902;
constexpr std::int64_t kRebuildBase = 42'998;                                   // + 214 per dot, 851 per energizer, 972 per score digit, 1,588 per life
constexpr std::int64_t kRebuildPerDot = 214, kRebuildPerEnergizer = 851, kRebuildPerDigit = 972, kRebuildPerLife = 1'588;
constexpr std::int64_t kReadyToLastBlit = 1'158;
constexpr std::int64_t kLastBlitToTick = 3'114'542;                             // the READY wait (033-2)
}  // namespace

DeathTimeline deathTimeline(const DeathTimingInput& in) {
    DeathTimeline t;
    const int s = ((in.pacX % 7) + 7) % 7;
    t.clear[0] = kFirstClear;
    for (std::size_t i = 1; i < 5; ++i) t.clear[i] = t.clear[i - 1] + kClearSteps[i - 1];
    t.image = t.clear[4] + kClearToImage;
    t.imageClear = t.image + kImageToImageClear;
    std::int64_t at = t.imageClear + kImageClearToFrame[s];
    for (std::size_t k = 0; k < 6; ++k) {
        t.frame[k] = at;
        t.sound[k] = at + kFrameToSound;
        t.frameClear[k] = at + kFrameToClear[k] + ((s >= 2 && s <= 4) ? kFrameToClearForShift234 : 0) + kPageCrossingMean;
        at = t.frameClear[k] + kClearToNextFrame[s];
    }
    t.rebuild = t.frameClear[5] + kLastClearToRebuild;
    t.ready = t.rebuild + kRebuildBase + kRebuildPerDot * in.dots + kRebuildPerEnergizer * in.energizers +
              kRebuildPerDigit * (in.scoreDigits + in.highDigits) + kRebuildPerLife * in.livesBefore + in.extraHud;
    t.lastBlit = t.ready + kReadyToLastBlit;
    t.total = t.lastBlit + kLastBlitToTick;
    return t;
}

}  // namespace pacman::core
