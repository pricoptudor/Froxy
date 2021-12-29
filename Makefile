all:
	gcc client-proxy.c ./sqlite/sqlite3.c ./cjson/cJSON.c -ldl -o proxy
	gcc client.c ./sqlite/sqlite3.c ./cjson/cJSON.c -ldl -o client
clean:
	rm -f *~server client
