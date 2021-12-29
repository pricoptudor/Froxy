all:
	gcc client-proxy.c ./sqlite/sqlite3.c ./cjson/cJSON.c -lpthread -ldl -o proxy
	gcc client.c ./sqlite/sqlite3.c ./cjson/cJSON.c -lpthread -ldl -o client
clean:
	rm -f *~server client
