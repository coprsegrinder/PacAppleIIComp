#!/bin/bash
# C++ LINE COVERAGE of the port (TASK-034-COVERAGE). Run from the repository root; needs the local recordings in scratch/ (as tools/full_suite.sh).
# Microsoft's dotnet-coverage was tried first and cannot measure a native program (its native instrumentation is a Visual Studio component); tools/linecov/linecov.cpp does it with the debugger API.
# 1. builds the test program with line information (port/build_coverage.bat: no optimisation, PDB) and the runner (tools/linecov/build.bat),
# 2. runs the tests under the runner with every optional input (a few times slower than tools/full_suite.sh), 3. summarises per source file (tools/coverage_report.py -> docs/CODE_COVERAGE_PORT.md).
cd "$(dirname "$0")/.." || exit 2
S=scratch
LOGIC=$(ls $S/ticks_[ABCD].bin $S/ticks_B10.bin $S/ticks_N00.bin $S/ticks_T00.bin $S/ticks_L[0-9][0-9].bin $S/ticks_E[0-9][0-9].bin $S/ticks_V[0-9][0-9].bin | tr '\n' ',' | sed 's/,$//')
DRAW=$(ls $S/ticks_[ABCDFGHJKLMPQRSU].bin | tr '\n' ',' | sed 's/,$//')
cmd //c "port\\build_coverage.bat" 2>&1 | grep -E "error C|fatal|LNK" | head -5
cmd //c "tools\\linecov\\build.bat" 2>&1 | grep -E "error C|fatal|LNK" | head -5
out/cov/linecov.exe '\port\src\' $S/linecov.tsv out/cov/pacman_tests.exe --image "Pacman (1983)(Atari).dsk" --reference "third_party/AppleWin/DOS 3.3 System Master - 680-0210-A.dsk" \
  --joyticks $S/ticks_JOY.bin --joydemo $S/joydemo/ticks_DEMO.bin --vectors $S/hires_vectors.bin --dots $S/dots_vectors.bin --script $S/cartoon1_script.bin --demo $S/ticks_DEMO.bin \
  --decide $S/decide_vectors.bin --page0 $S/page_tick0.bin --clicks $S/sound_clicks.bin --deaths $S/death_timing.bin --titleevents $S/title_events.bin --chaseevents $S/chase_events.bin --titlesnaps $S/title_snaps.bin --setupevents $S/demosetup_events.bin --endevents $S/demoend_events.bin --demopages $S/pages_DEMOF.bin --democycles $S/demo_cycles.bin --prompts $S/prompts.bin --hidden $S/hidden.bin --boot $S/boot.bin --playerpages $S/page_players_1.bin,$S/page_players_2.bin,$S/page_players_3.bin,$S/page_players_4.bin,$S/page_players_5.bin --demoafter $S/demo_after_1_1.txt,$S/demo_after_3_1.txt,$S/demo_after_2_5.txt,$S/demo_after_1_9.txt,$S/demo_after_5_3.txt,$S/demo_after_3_0.txt --playerticks $S/ticks_P.bin,$S/ticks_Q.bin,$S/ticks_R.bin,$S/ticks_S.bin,$S/ticks_U.bin --charrom "Apple II roms/Apple II Character ROM - 341-0036.bin" --ticks "$LOGIC" --drawticks "$DRAW" > $S/coverage_cpp.log 2>&1
grep -E "FAIL|SKIPPED|tests," $S/coverage_cpp.log | cut -c1-200
python -I tools/coverage_report.py
