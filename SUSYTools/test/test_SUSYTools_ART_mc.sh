#!/bin/sh

# art-description: SUSYTools ART test - share/minimalExampleJobOptions_mc.py
# art-type: build
# art-ci: 21.2
# art-include: 21.2/AthAnalysis

echo "Running SUSYTools test: \'share/minimalExampleJobOptions_mc.py\'"
athena SUSYTools/minimalExampleJobOptions_mc.py

result=$?
echo "Done. Exit code is "$result
exit $result
