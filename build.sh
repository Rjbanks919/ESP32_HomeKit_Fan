#!/bin/bash

ESPPORT=/dev/ttyUSB0

# Main function for the build script
main () {
	# Check whether ESP IDF is ready for this shell
	if ! [[ -v ESP_IDF_VERSION ]]; then
		echo -e "\n[ERROR] ESP-IDF not init...\n"
		echo -e "[INFO] Run \". \$HOME/esp/esp-idf/export.(sh|fish)\""
		exit 1
	fi

	# Move into the working directory
	pushd ./lasko_homekit_fan > /dev/null

    parse_args $@

    # Print out the fancy logo
	cat ../images/logo.txt

	# Monitor if asked
	if [[ -v monitor ]]; then
		idf.py monitor
	elif [[ -v flash ]]; then
		idf.py flash
	elif [[ -v oneshot ]]; then
		idf.py build flash monitor
	else
		idf.py build
	fi

	# Move back to our starting directory
	popd > /dev/null
}

# Function to parse command line arguments
parse_args () {
    while [[ $# -gt 0 ]]; do
	case $1 in
	    -c | --clean)
		clean_workspace
		shift;;
	    -m | --monitor)
		monitor=1
		shift;;
	    -f | --flash)
		flash=1
		shift;;
		-o | --oneshot)
		oneshot=1
		shift;;
	    *)
		echo "Unknown option $1"
		exit 1
		shift;;
	esac
    done
}

# Function to clean the workspace
clean_workspace () {
	echo -e "\n[INFO] Cleaning workspace ...\n"
	idf.py clean
	idf.py fullclean
	rm -rf ./build/
	exit 0
}

# Kick off the main function
main $@
