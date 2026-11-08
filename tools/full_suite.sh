#!/bin/bash
# FULL-SUITE CHECKPOINT (run from the repository root; needs the local git-ignored recordings in scratch/).
# 1. builds the C++ tests (port/build.bat), 2. runs them with every optional input, 3. runs the Python tests.
# --ticks      : recordings valid for the logic tests (golden A-D, B10, tunnel N00/T00, levels L04-L21, level-end E04 E05 E09 E20)
# --drawticks  : recordings for the drawing test (A-D plus the poked/custom scenarios F G H J K L M P Q R S with call logs)
# Usage: bash tools/full_suite.sh [python]   ("python" adds the Python tests, several minutes)
cd "$(dirname "$0")/.." || exit 2
S=scratch
LOGIC=$(ls $S/ticks_[ABCD].bin $S/ticks_B10.bin $S/ticks_N00.bin $S/ticks_T00.bin $S/ticks_L[0-9][0-9].bin $S/ticks_E[0-9][0-9].bin $S/ticks_V[0-9][0-9].bin $S/ticks_W[0-9][0-9].bin | tr '\n' ',' | sed 's/,$//')
DRAW=$(ls $S/ticks_[ABCDFGHJKLMPQRSU].bin | tr '\n' ',' | sed 's/,$//')
cmd //c "port\build.bat" 2>&1 | grep -E "error|warning" | head -5
out/bat/pacman_tests.exe --image "Pacman (1983)(Atari).dsk" --reference "third_party/AppleWin/DOS 3.3 System Master - 680-0210-A.dsk" \
  --joyticks $S/ticks_JOY.bin --joydemo $S/joydemo/ticks_DEMO.bin --vectors $S/hires_vectors.bin --dots $S/dots_vectors.bin --script $S/cartoon1_script.bin --demo $S/ticks_DEMO.bin \
  --decide $S/decide_vectors.bin --page0 $S/page_tick0.bin --clicks $S/sound_clicks.bin --deaths $S/death_timing.bin --titleevents $S/title_events.bin --chaseevents $S/chase_events.bin --titlesnaps $S/title_snaps.bin --setupevents $S/demosetup_events.bin --endevents $S/demoend_events.bin --demopages $S/pages_DEMOF.bin --democycles $S/demo_cycles.bin --prompts $S/prompts.bin --hidden $S/hidden.bin --boot $S/boot.bin --playerpages $S/page_players_1.bin,$S/page_players_2.bin,$S/page_players_3.bin,$S/page_players_4.bin,$S/page_players_5.bin --demoafter $S/demo_after_1_1.txt,$S/demo_after_3_1.txt,$S/demo_after_2_5.txt,$S/demo_after_1_9.txt,$S/demo_after_5_3.txt,$S/demo_after_3_0.txt --playerticks $S/ticks_P.bin,$S/ticks_Q.bin,$S/ticks_R.bin,$S/ticks_S.bin,$S/ticks_U.bin --charrom "Apple II roms/Apple II Character ROM - 341-0036.bin" --ticks "$LOGIC" --drawticks "$DRAW" > $S/full_suite_cpp.log 2>&1
grep -E "FAIL|SKIPPED|tests," $S/full_suite_cpp.log | cut -c1-200
if [ "$1" = "python" ]; then
  # 40 test files, each boots the game in Python (about 2-3 minutes alone): 6 at a time, about 25 minutes in total on 12 cores
  ls tests/test_*.py | xargs -P 6 -I{} sh -c 'python -I {} > '$S'/py_$(basename {}).log 2>&1 && echo "OK   {}" || echo "FAIL {}"'
fi
