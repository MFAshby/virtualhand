while(true) 
do
	inotifywait -qq -e modify -e move -e create --exclude .*.swp -r src
	pebble build
	if [[ $?==0 ]]; then
		pebble install --phone 192.168.1.66
	fi
done
