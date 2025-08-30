#!/bin/sh

# Aborts on error
set -e

# Gets output file from command line arguments
outfile=$1
case "$outfile" in
	# Continues on valid argument pattern
	./*);;
	/*);;
	# Ensures output file is defined
	"")
		echo "Usage: $0 <out-file> key=value..."
		exit 1
		;;
	# Ensures output file is an absolute or relative path
	*)
		echo "First argument is not an absolute or relative path"
		exit 1
		;;
esac
shift

# Ensures output file does not already exist
if [ -f "$outfile" ]; then
	read -p "Output file already exists, do you want to overwrite it? [y/n] " answer
	case $answer in
		[Yy])
			;;
		*)
			exit 1;;
	esac
fi

# Generates preset file from command line configuration
echo "#!/usr/bin/env sh" > $outfile
printf 'curl "$1" --silent --show-error --max-time 5' >> $outfile
for conf in "$@"; do
	printf " \\\\\n\t--data-urlencode '$(sed s/\'/\'\\\\\\\\\'\'/ <<< $conf)'" >> $outfile
done
echo "" >> $outfile
echo 'echo ""' >> $outfile

# Makes output file executable
chmod +x "$outfile"
