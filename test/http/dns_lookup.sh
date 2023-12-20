#!/usr/bin/env bash

# Ensures device address is set
if [[ $DEVICE_ADDR == "" ]]; then
	echo "Environment variable DEVICE_ADDR not defined"
	exit 1
fi

# Configures script to exit on error if runner mode is abort
case $RUNNER_MODE in
	"abort")
		set -e
		;;
	"" | "all")
		;;
	*)
		echo "Invalid RUNNER_MODE value: $RUNNER_MODE"
		exit 1
		;;
esac

tests=0
fails=0

# Runs all combinations of dig flags for DNS request
for query in "google.com" "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" ""; do
	for class in "IN" "ANY"; do
		for type in "A" "ANY"; do
			for rd in "recurse" "norecurse"; do
				for ad in "adflag" "noadflag"; do
					for cd in "cdflag" "nocdflag"; do
						for aa in "aaflag" "noaaflag"; do
							echo "Test started: $aa $cd $ad $rd $type $class '$query'"
							ret=$(dig @$DEVICE_ADDR +short -c $class -t $type -q $query +$rd +time=1)
							if [[ $ret != $DEVICE_ADDR ]]; then
								echo "ERROR: Received unexpected response"
								echo "	Response: '$ret'"
								echo "	Expected: '$DEVICE_ADDR'"
								((fails++))
							fi
							((tests++))
						done
					done
				done
			done
		done
	done
done

echo "Tests failed: $fails/$tests"
exit $fails
