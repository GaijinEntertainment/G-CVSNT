#pragma once
//will start (almost) infinite loop, unless should_stop == true
bool start_push_server(int portno, int max_connections, volatile bool *should_stop);
